/*
 * tuner.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "discover.h"
#include "tuner.h"

cSatipTuner::cSatipTuner(cSatipDeviceIf &deviceP, unsigned int packetLenP)
: cThread(cString::sprintf("SAT>IP tuner %d", deviceP.GetId())),
  dataThreadM(deviceP, *this, packetLenP),
  sleepM(),
  deviceM(&deviceP),
  deviceIdM(deviceM ? deviceM->GetId() : -1),
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
  timeoutM(eMinKeepAliveIntervalMs),
  reconnectM(false),
  tunedM(false),
  hasLockM(false),
  signalStrengthM(-1),
  signalQualityM(-1),
  streamIdM(-1),
  addPidsM(),
  delPidsM(),
  pidsM()
{
  debug("cSatipTuner::%s(%d) [device %d]", __FUNCTION__, packetLenP, deviceIdM);

  RtspInitialize();

  bool waitSocket = false;

  // Open sockets
  for (int i = 100; i > 0; --i) {
      if (rtpSocketM->Open(0, waitSocket) &&
          rtcpSocketM->Open(rtpSocketM->Port() + 1), waitSocket)
         break;
      rtpSocketM->Close();
      rtcpSocketM->Close();
      }

  if ((rtpSocketM->Port() <= 0) || (rtcpSocketM->Port() <= 0)) {
     error("Cannot open required RTP/RTCP ports [device %d]", deviceIdM);
     }
  else
     info("Using RTP/RTCP ports %d-%d [device %d]", rtpSocketM->Port(), rtcpSocketM->Port(), deviceIdM);

  // Start threads
  dataThreadM.Start(rtpSocketM);
  Start();
}

cSatipTuner::~cSatipTuner()
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);

  // Stop threads
  dataThreadM.Cancel(3);
  sleepM.Signal();
  if (Running())
     Cancel(3);
  Close();

  // Terminate curl session
  RtspTerminate();

  // Close the listening sockets
  rtpSocketM->Close();
  rtcpSocketM->Close();

  // Free allocated memory
  DELETENULL(rtcpSocketM);
  DELETENULL(rtpSocketM);
}

void cSatipTuner::Action(void)
{
  debug("cSatipTuner::%s(): entering [device %d]", __FUNCTION__, deviceIdM);
  cTimeMs rtcpTimeout(eReConnectTimeoutMs);
  // Increase priority
  SetPriority(-1);
  // Do the thread loop
  while (Running()) {
        if (reconnectM) {
           info("SAT>IP Device %d reconnecting.", deviceM->GetId());
           cMutexLock MutexLock(&mutexM);
           if (tunedM)
              Disconnect();
           Connect();
           reconnectM = false;
           }
        if (tunedM) {
           // Update pids
           UpdatePids();
           // Remember the heart beat
           KeepAlive();
           // Read reception statistics via DESCRIBE
           if (!pidsM.Size() && statusUpdateM.TimedOut() ) {
              cString uri = cString::sprintf("rtsp://%s/stream=%d",
                                             *streamAddrM, streamIdM);
              cMutexLock MutexLock(&mutexM);
              if (rtspM->Describe(*uri)) 
                 statusUpdateM.Set(eStatusUpdateTimeoutMs);
              }
           // Read reception statistics via RTCP
           if (rtcpSocketM && rtcpSocketM->IsOpen()) {
              unsigned char buf[1450];
              memset(buf, 0, sizeof(buf));
              if (rtcpSocketM->ReadApplication(buf, sizeof(buf)) > 0) {
                 ParseReceptionParameters((const char *)buf);
                 rtcpTimeout.Set(eReConnectTimeoutMs);
                 }
              }
           // Quirk for devices without valid reception data
           if (currentServerM && currentServerM->Quirk(cSatipServer::eSatipQuirkForceLock)) {
              hasLockM = true;
              signalStrengthM = eDefaultSignalStrength;
              signalQualityM = eDefaultSignalQuality;
              }

           if (rtcpTimeout.TimedOut()) {
              error("No RTP Data received for %d ms [device %d], Reconnect initiated",
                    (int)rtcpTimeout.Elapsed(), deviceM->GetId());
              rtcpTimeout.Set(eReConnectTimeoutMs);
              reconnectM = true;
              }

           int passedMs = dataThreadM.LastReceivedMs();
           if (passedMs >= eReConnectTimeoutMs) {
              error("No RTP Data received for %d ms [device %d], Reconnect initiated",
                    (int)passedMs, deviceM->GetId());
              dataThreadM.ResetLastReceivedMs();
              reconnectM = true;
              }
           }
        sleepM.Wait(50); // to avoid busy loop and reduce cpu load
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

  if (isempty(*streamAddrM)) {
     if (tunedM)
        Disconnect();
     tunedM = false;
     return tunedM;
     }

  // Setup stream
  cString connectionUri = cString::sprintf("rtsp://%s", *streamAddrM);
  cString uri = cString::sprintf("%s/?%s", *connectionUri, *streamParamM);
  tunedM = rtspM->Setup(*uri, rtpSocketM->Port(), rtcpSocketM->Port());
  if (!tunedM)
     return tunedM;
  if (nextServerM && nextServerM->Quirk(cSatipServer::eSatipQuirkSessionId))
     rtspM->SetSession(SkipZeroes(*sessionM));

  dataThreadM.Flush();
  keepAliveM.Set(timeoutM);

  // Start playing
  UpdatePids(true);
  if (nextServerM) {
     cSatipDiscover::GetInstance()->UseServer(nextServerM, true);
     currentServerM = nextServerM;
     nextServerM = NULL;
     }

  return tunedM;
}

bool cSatipTuner::Disconnect(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);

  // Teardown rtsp session
  if (tunedM) {
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
     rtspM->Teardown(*uri);
     streamIdM = -1;
     tunedM = false;
     }

  // Reset signal parameters
  hasLockM = false;
  signalStrengthM = -1;
  signalQualityM = -1;

  if (currentServerM)
     cSatipDiscover::GetInstance()->UseServer(currentServerM, false);
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
        value = !!atoi(++c);
        if (value != hasLockM)
           info("Device %d %s lock", deviceM->GetId(), hasLockM ? "gained" : "lost");
        hasLockM = value;

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
     if (nextServerM && !isempty(nextServerM->Address()) && !isempty(parameterP)) {
        // Update stream address and parameter
        streamAddrM = rtspM->RtspUnescapeString(nextServerM->Address());
        streamParamM = parameterP;
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
  debug("cSatipTuner::%s(%d, %d, %d) [device %d]", __FUNCTION__, pidP, typeP, onP, deviceIdM);
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

cString cSatipTuner::GeneratePidParameter(bool allPidsP)
{
  debug("cSatipTuner::%s(%d) [device %d]", __FUNCTION__, (int)allPidsP, deviceM->GetId());
  cMutexLock MutexLock(&mutexM);

  bool usedummy = !!(currentServerM && currentServerM->Quirk(cSatipServer::eSatipQuirkPlayPids));
  cString param = "";

  if (allPidsP || usedummy) {
     if (usedummy && (pidsM.Size() == 1) && (pidsM[0] < 0x20))
        param = cString::sprintf("%s,%d", *param, eDummyPid);
     else if (pidsM.Size()) {
        param = "pids=";
        for (int i = 0; i < pidsM.Size(); ++i)
            param = cString::sprintf("%s%s%d", *param, (i == 0 ? "" : ","), pidsM[i]);
        }
     }
  else {
     if (addPidsM.Size()) {
        param = "addpids=";
        for (int i = 0; i < addPidsM.Size(); ++i)
            param = cString::sprintf("%s%s%d", *param, (i == 0 ? "" : ","), addPidsM[i]);
        addPidsM.Clear();
        }
     if (delPidsM.Size()) {
        param = cString::sprintf("%s%sdelpids=", *param, (isempty(*param) ? "" : "&"));
        for (int i = 0; i < delPidsM.Size(); ++i)
            param = cString::sprintf("%s%s%d", *param, (i == 0 ? "" : ","), delPidsM[i]);
        delPidsM.Clear();
        }
     }

   return param;
}

bool cSatipTuner::UpdatePids(bool forceP)
{
  cMutexLock MutexLock(&mutexM);
  //debug("cSatipTuner::%s(%d) [device %d]", __FUNCTION__, (int)forceP, deviceM->GetId());

  if ( ( (forceP && pidsM.Size()) ||
         (pidUpdateCacheM.TimedOut() && (addPidsM.Size() || delPidsM.Size())) ) &&
       tunedM && !isempty(*streamAddrM)) {

     cString uri = cString::sprintf("rtsp://%s/stream=%d?%s",
                                    *streamAddrM, streamIdM,
                                    *GeneratePidParameter(forceP));

     // Disable RTP Timeout while sending PLAY Command
     if (RtspPlay(*uri)) {
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
     debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceIdM);
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);

     if (rtspM->Options(*uri)) {
        keepAliveM.Set(timeoutM);
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
