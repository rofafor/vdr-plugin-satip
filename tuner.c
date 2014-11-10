/*
 * tuner.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <sys/epoll.h>

#include "common.h"
#include "config.h"
#include "discover.h"
#include "tuner.h"

cSatipTuner::cSatipTuner(cSatipDeviceIf &deviceP, unsigned int packetLenP)
: cThread(cString::sprintf("SAT>IP %d tuner", deviceP.GetId())),
  sleepM(),
  deviceM(&deviceP),
  deviceIdM(deviceP.GetId()),
  packetBufferLenM(packetLenP),
  rtspM(new cSatipRtsp(*this)),
  rtpSocketM(new cSatipSocket()),
  rtcpSocketM(new cSatipSocket()),
  streamAddrM(""),
  streamParamM(""),
  currentServerM(NULL),
  nextServerM(NULL),
  mutexM(),
  keepAliveM(),
  statusUpdateM(),
  pidUpdateCacheM(),
  sessionM(""),
  fdM(epoll_create(eMaxFileDescriptors)),
  timeoutM(eMinKeepAliveIntervalMs),
  openedM(false),
  tunedM(false),
  hasLockM(false),
  signalStrengthM(-1),
  signalQualityM(-1),
  streamIdM(-1),
  addPidsM(),
  delPidsM(),
  pidsM()
{
  debug("cSatipTuner::%s(%d) [device %d]", __FUNCTION__, packetBufferLenM, deviceIdM);
  // Allocate packet buffer
  packetBufferM = MALLOC(unsigned char, packetBufferLenM);
  if (packetBufferM)
     memset(packetBufferM, 0, packetBufferLenM);
  else
     error("MALLOC() failed for packet buffer [device %d]", deviceIdM);

  // Open sockets
  int i = 100;
  while (i-- > 0) {
        if (rtpSocketM->Open(0) && rtcpSocketM->Open(rtpSocketM->Port() + 1))
           break;
        rtpSocketM->Close();
        rtcpSocketM->Close();
        }
  if ((rtpSocketM->Port() <= 0) || (rtcpSocketM->Port() <= 0)) {
     error("Cannot open required RTP/RTCP ports [device %d]", deviceIdM);
     }

  // Setup epoll
  if (fdM >= 0) {
     if (rtpSocketM->Fd() >= 0) {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = rtpSocketM->Fd();
        if (epoll_ctl(fdM, EPOLL_CTL_ADD, rtpSocketM->Fd(), &ev) == -1) {
           error("Cannot add RTP socket into epoll [device %d]", deviceIdM);
           }
        }
     if (rtcpSocketM->Fd() >= 0) {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = rtcpSocketM->Fd();
        if (epoll_ctl(fdM, EPOLL_CTL_ADD, rtcpSocketM->Fd(), &ev) == -1) {
           error("Cannot add RTP socket into epoll [device %d]", deviceIdM);
           }
        }
     }

  // Start thread
  Start();
}

cSatipTuner::~cSatipTuner()
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
  // Stop thread
  sleepM.Signal();
  if (Running())
     Cancel(3);
  Close();

  // Cleanup epoll
  if (fdM >= 0) {
     if ((rtpSocketM->Fd() >= 0) && (epoll_ctl(fdM, EPOLL_CTL_DEL, rtpSocketM->Fd(), NULL) == -1)) {
        error("Cannot remove RTP socket from epoll [device %d]", deviceIdM);
        }
     if ((rtcpSocketM->Fd() >= 0) && (epoll_ctl(fdM, EPOLL_CTL_DEL, rtcpSocketM->Fd(), NULL) == -1)) {
        error("Cannot remove RTP socket from epoll [device %d]", deviceIdM);
        }
     close(fdM);
     }

  // Close the listening sockets
  rtpSocketM->Close();
  rtcpSocketM->Close();

  // Free allocated memory
  free(packetBufferM);
  DELETENULL(rtcpSocketM);
  DELETENULL(rtpSocketM);
  DELETENULL(rtspM);
}

void cSatipTuner::Action(void)
{
  debug("cSatipTuner::%s(): entering [device %d]", __FUNCTION__, deviceIdM);
  cTimeMs timeout(eReConnectTimeoutMs);
  // Increase priority
  SetPriority(-1);
  // Do the thread loop
  while (packetBufferM && rtpSocketM && rtcpSocketM && Running()) {
        struct epoll_event events[eMaxFileDescriptors];
        int nfds = epoll_wait(fdM, events, eMaxFileDescriptors, eReadTimeoutMs);
        switch (nfds) {
          default:
               for (int i = 0; i < nfds; ++i) {
                   timeout.Set(eReConnectTimeoutMs);
                   if (events[i].data.fd == rtpSocketM->Fd()) {
                      // Read data
                      int length = rtpSocketM->ReadVideo(packetBufferM, min(deviceM->CheckData(), packetBufferLenM));
                      if (length > 0) {
                         AddTunerStatistic(length);
                         deviceM->WriteData(packetBufferM, length);
                         }
                      }
                   else if (events[i].data.fd == rtcpSocketM->Fd()) {
                      unsigned char buf[1450];
                      memset(buf, 0, sizeof(buf));
                      if (rtcpSocketM->ReadApplication(buf, sizeof(buf)) > 0)
                         ParseReceptionParameters((const char *)buf);
                      }
                   }
               // fall through!
          case 0:
               // Update pids
               UpdatePids();
               // Remember the heart beat
               KeepAlive();
               // Read reception statistics via DESCRIBE and RTCP
               if (ReadReceptionStatus()) {
                  // Quirk for devices without valid reception data
                  if (currentServerM && currentServerM->Quirk(cSatipServer::eSatipQuirkForceLock)) {
                     hasLockM = true;
                     signalStrengthM = eDefaultSignalStrength;
                     signalQualityM = eDefaultSignalQuality;
                     }
                  }
               // Reconnect if necessary
               if (openedM && timeout.TimedOut()) {
                  Disconnect();
                  Connect();
                  timeout.Set(eReConnectTimeoutMs);
                  }
               break;
          case -1:
               error("epoll_wait() failed");
               break;
           }
        }
  debug("cSatipTuner::%s(): exiting [device %d]", __FUNCTION__, deviceIdM);
}

bool cSatipTuner::Open(void)
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
  return Connect();
}

bool cSatipTuner::Close(void)
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
  return Disconnect();
}

bool cSatipTuner::Connect(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);

  if (!isempty(*streamAddrM)) {
     cString connectionUri = cString::sprintf("rtsp://%s", *streamAddrM);
     cString uri = cString::sprintf("%s/?%s", *connectionUri, *streamParamM);
     // Just retune
     if (tunedM && (streamIdM >= 0)) {
        debug("cSatipTuner::%s(): retune [device %d]", __FUNCTION__, deviceIdM);
        keepAliveM.Set(0);
        KeepAlive();
        // Flush any old content
        if (rtpSocketM)
           rtpSocketM->Flush();
        openedM = rtspM->Setup(*uri, rtpSocketM->Port(), rtcpSocketM->Port());
        return openedM;
        }
     keepAliveM.Set(timeoutM);
     openedM = rtspM->Options(*connectionUri);
     if (openedM) {
        if (nextServerM && nextServerM->Quirk(cSatipServer::eSatipQuirkSessionId))
           rtspM->SetSession(SkipZeroes(*sessionM));
        if (rtspM->Setup(*uri, rtpSocketM->Port(), rtcpSocketM->Port())) {
           tunedM = true;
           UpdatePids(true);
           if (nextServerM) {
              cSatipDiscover::GetInstance()->UseServer(nextServerM, true);
              currentServerM = nextServerM;
              nextServerM = NULL;
              }
           }
        else
           openedM = false;
        }

     return openedM;
     }

  openedM = false;
  return openedM;
}

bool cSatipTuner::Disconnect(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);

  if (openedM && !isempty(*streamAddrM)) {
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
     rtspM->Teardown(*uri);
     }

  // Reset signal parameters
  hasLockM = false;
  signalStrengthM = -1;
  signalQualityM = -1;

  if (currentServerM)
     cSatipDiscover::GetInstance()->UseServer(currentServerM, false);
  openedM = false;
  tunedM = false;
  statusUpdateM.Set(0);
  timeoutM = eMinKeepAliveIntervalMs;
  addPidsM.Clear();
  delPidsM.Clear();

  // return always true
  return true;
}

void cSatipTuner::ParseReceptionParameters(const char *paramP)
{
  //debug("cSatipTuner::%s(%s) [device %d]", __FUNCTION__, paramP, deviceIdM);
  // DVB-S2:
  // ver=<major>.<minor>;src=<srcID>;tuner=<feID>,<level>,<lock>,<quality>,<frequency>,<polarisation>,<system>,<type>,<pilots>,<roll_off>,<symbol_rate>,<fec_inner>;pids=<pid0>,...,<pidn>
  // DVB-T2:
  // ver=1.1;tuner=<feID>,<level>,<lock>,<quality>,<freq>,<bw>,<msys>,<tmode>,<mtype>,<gi>,<fec>,<plp>,<t2id>,<sm>;pids=<pid0>,...,<pidn>
  if (!isempty(paramP)) {
     char *s = strdup(paramP);
     char *c = strstr(s, ";tuner=");
     if (c)  {
        int value;

        // level:
        // Numerical value between 0 and 255
        // An incoming L-band satellite signal of
        // -25dBm corresponds to 224
        // -65dBm corresponds to 32
        // No signal corresponds to 0
        c = strstr(c, ",");
        value = atoi(++c);
        // Scale value to 0-100
        signalStrengthM = (value >= 0) ? (value * 100 / 255) : -1;

        // lock:
        // lock Set to one of the following values:
        // "0" the frontend is not locked
        // "1" the frontend is locked
        c = strstr(c, ",");
        hasLockM = !!atoi(++c);

        // quality:
        // Numerical value between 0 and 15
        // Lowest value corresponds to highest error rate
        // The value 15 shall correspond to
        // -a BER lower than 2x10-4 after Viterbi for DVB-S
        // -a PER lower than 10-7 for DVB-S2
        c = strstr(c, ",");
        value = atoi(++c);
        // Scale value to 0-100
        signalQualityM = (hasLockM && (value >= 0)) ? (value * 100 / 15) : 0;
        }
     free(s);
     }
}

void cSatipTuner::SetStreamId(int streamIdP)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s(%d) [device %d]", __FUNCTION__, streamIdP, deviceIdM);
  streamIdM = streamIdP;
}

void cSatipTuner::SetSessionTimeout(const char *sessionP, int timeoutP)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s(%s, %d) [device %d]", __FUNCTION__, sessionP, timeoutP, deviceIdM);
  sessionM = sessionP;
  timeoutM = (timeoutP > eMinKeepAliveIntervalMs) ? timeoutP : eMinKeepAliveIntervalMs;
}

int cSatipTuner::GetId(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
  return deviceIdM;
}

bool cSatipTuner::SetSource(cSatipServer *serverP, const char *parameterP, const int indexP)
{
  debug("cSatipTuner::%s(%s, %d) [device %d]", __FUNCTION__, parameterP, indexP, deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (serverP) {
     nextServerM = cSatipDiscover::GetInstance()->GetServer(serverP);
     if (rtspM && nextServerM && !isempty(nextServerM->Address()) && !isempty(parameterP)) {
        // Update stream address and parameter
        streamAddrM = rtspM->RtspUnescapeString(nextServerM->Address());
        streamParamM = rtspM->RtspUnescapeString(parameterP);
        // Reconnect
        Connect();
        }
     }
  else
     Disconnect();
  return true;
}

bool cSatipTuner::SetPid(int pidP, int typeP, bool onP)
{
  //debug("cSatipTuner::%s(%d, %d, %d) [device %d]", __FUNCTION__, pidP, typeP, onP, deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (onP) {
     pidsM.AddPid(pidP);
     addPidsM.AddPid(pidP);
     delPidsM.RemovePid(pidP);
     }
  else {
     pidsM.RemovePid(pidP);
     delPidsM.AddPid(pidP);
     addPidsM.RemovePid(pidP);
     }
  pidUpdateCacheM.Set(ePidUpdateIntervalMs);
  return true;
}

bool cSatipTuner::UpdatePids(bool forceP)
{
  cMutexLock MutexLock(&mutexM);
  if (((forceP && pidsM.Size()) || (pidUpdateCacheM.TimedOut() && (addPidsM.Size() || delPidsM.Size()))) &&
      tunedM && !isempty(*streamAddrM) && (streamIdM > 0)) {
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
     bool usedummy = !!(currentServerM && currentServerM->Quirk(cSatipServer::eSatipQuirkPlayPids));
     if (forceP || usedummy) {
        if (pidsM.Size()) {
           uri = cString::sprintf("%s?pids=", *uri);
           for (int i = 0; i < pidsM.Size(); ++i)
               uri = cString::sprintf("%s%d%s", *uri, pidsM[i], (i == (pidsM.Size() - 1)) ? "" : ",");
           }
        if (usedummy && (pidsM.Size() == 1) && (pidsM[0] < 0x20))
           uri = cString::sprintf("%s,%d", *uri, eDummyPid);
        }
     else {
        if (addPidsM.Size()) {
           uri = cString::sprintf("%s?addpids=", *uri);
           for (int i = 0; i < addPidsM.Size(); ++i)
               uri = cString::sprintf("%s%d%s", *uri, addPidsM[i], (i == (addPidsM.Size() - 1)) ? "" : ",");
           }
        if (delPidsM.Size()) {
           uri = cString::sprintf("%s%sdelpids=", *uri, addPidsM.Size() ? "&" : "?");
           for (int i = 0; i < delPidsM.Size(); ++i)
               uri = cString::sprintf("%s%d%s", *uri, delPidsM[i], (i == (delPidsM.Size() - 1)) ? "" : ",");
           }
        }
     if (rtspM->Play(*uri)) {
        addPidsM.Clear();
        delPidsM.Clear();
        return true;
        }
     Disconnect();
     }

  return false;
}

bool cSatipTuner::KeepAlive(void)
{
  cMutexLock MutexLock(&mutexM);
  if (tunedM && keepAliveM.TimedOut()) {
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
     keepAliveM.Set(timeoutM);
     if (rtspM->Options(*uri))
        return true;
     Disconnect();
     }

  return false;
}

bool cSatipTuner::ReadReceptionStatus(void)
{
  cMutexLock MutexLock(&mutexM);
  if (tunedM && !pidsM.Size() && statusUpdateM.TimedOut() ) {
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
     if (rtspM->Describe(*uri)) {
        statusUpdateM.Set(eStatusUpdateTimeoutMs);
        return true;
        }
     Disconnect();
     }

  return false;
}

int cSatipTuner::SignalStrength(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
  return signalStrengthM;
}

int cSatipTuner::SignalQuality(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
  return signalQualityM;
}

bool cSatipTuner::HasLock(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
  return tunedM && hasLockM;
}

cString cSatipTuner::GetSignalStatus(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
  return cString::sprintf("lock=%d strength=%d quality=%d", HasLock(), SignalStrength(), SignalQuality());
}

cString cSatipTuner::GetInformation(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
  return tunedM ? cString::sprintf("rtsp://%s/?%s [stream=%d]", *streamAddrM, *streamParamM, streamIdM) : "connection failed";
}
