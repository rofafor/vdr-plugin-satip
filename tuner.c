/*
 * tuner.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "discover.h"
#include "poller.h"
#include "tuner.h"

cSatipTuner::cSatipTuner(cSatipDeviceIf &deviceP, unsigned int packetLenP)
: cThread(cString::sprintf("SAT>IP %d tuner", deviceP.GetId())),
  sleepM(),
  deviceM(&deviceP),
  deviceIdM(deviceP.GetId()),
  rtspM(*this),
  rtpM(*this, packetLenP),
  rtcpM(*this, eApplicationMaxSizeB),
  streamAddrM(""),
  streamParamM(""),
  currentServerM(NULL),
  nextServerM(NULL),
  mutexM(),
  reConnectM(),
  keepAliveM(),
  statusUpdateM(),
  pidUpdateCacheM(),
  sessionM(""),
  currentStateM(tsIdle),
  internalStateM(),
  externalStateM(),
  timeoutM(eMinKeepAliveIntervalMs),
  hasLockM(false),
  signalStrengthM(-1),
  signalQualityM(-1),
  streamIdM(-1),
  addPidsM(),
  delPidsM(),
  pidsM()
{
  debug("%s(, %d) [device %d]", __PRETTY_FUNCTION__, packetLenP, deviceIdM);

  // Open sockets
  int i = 100;
  while (i-- > 0) {
        if (rtpM.Open(0) && rtcpM.Open(rtpM.Port() + 1))
           break;
        rtpM.Close();
        rtcpM.Close();
        }
  if ((rtpM.Port() <= 0) || (rtcpM.Port() <= 0)) {
     error("Cannot open required RTP/RTCP ports [device %d]", deviceIdM);
     }
  // Must be done after socket initialization!
  cSatipPoller::GetInstance()->Register(rtpM);
  cSatipPoller::GetInstance()->Register(rtcpM);

  // Start thread
  Start();
}

cSatipTuner::~cSatipTuner()
{
  debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  // Stop thread
  sleepM.Signal();
  if (Running())
     Cancel(3);
  Close();
  currentStateM = tsIdle;
  internalStateM.Clear();
  externalStateM.Clear();

  // Close the listening sockets
  cSatipPoller::GetInstance()->Unregister(rtcpM);
  cSatipPoller::GetInstance()->Unregister(rtpM);
  rtcpM.Close();
  rtpM.Close();
}

void cSatipTuner::Action(void)
{
  debug("%s Entering [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  reConnectM.Set(eConnectTimeoutMs);
  // Do the thread loop
  while (Running()) {
        UpdateCurrentState();
        switch (currentStateM) {
          case tsIdle:
               //debug("%s: tsIdle [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               break;
          case tsRelease:
               //debug("%s: tsRelease [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               Disconnect();
               RequestState(tsIdle, smInternal);
               break;
          case tsSet:
               //debug("%s: tsSet [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               reConnectM.Set(eConnectTimeoutMs);
               if (Connect()) {
                  RequestState(tsTuned, smInternal);
                  UpdatePids(true);
                  }
               else {
                  error("Tuning failed - retuning [device %d]", deviceIdM);
                  Disconnect();
                  }
               break;
          case tsTuned:
               //debug("%s: tsTuned [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               reConnectM.Set(eConnectTimeoutMs);
               // Read reception statistics via DESCRIBE and RTCP
               if (hasLockM || ReadReceptionStatus()) {
                  // Quirk for devices without valid reception data
                  if (currentServerM && currentServerM->Quirk(cSatipServer::eSatipQuirkForceLock)) {
                     hasLockM = true;
                     signalStrengthM = eDefaultSignalStrength;
                     signalQualityM = eDefaultSignalQuality;
                     }
                  if (hasLockM)
                     RequestState(tsLocked, smInternal);
                  }
               break;
          case tsLocked:
               //debug("%s: tsLocked [device %d]", __PRETTY_FUNCTION__, deviceIdM);
               if (!UpdatePids()) {
                  error("Pid update failed - retuning [device %d]", deviceIdM);
                  RequestState(tsSet, smInternal);
                  break;
                  }
               if (!KeepAlive()) {
                  error("Keep-alive failed - retuning [device %d]", deviceIdM);
                  RequestState(tsSet, smInternal);
                  break;
                  }
               if (reConnectM.TimedOut()) {
                  error("Connection timeout - retuning [device %d]", deviceIdM);
                  RequestState(tsSet, smInternal);
                  break;
                  }
               break;
          default:
               error("Unknown tuner status %d [device %d]", currentStateM, deviceIdM);
               break;
          }
        if (!StateRequested())
           sleepM.Wait(eSleepTimeoutMs); // to avoid busy loop and reduce cpu load
        }
  debug("%s Exiting [device %d]", __PRETTY_FUNCTION__, deviceIdM);
}

bool cSatipTuner::Open(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  RequestState(tsSet, smExternal);

  // return always true
  return true;
}

bool cSatipTuner::Close(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  RequestState(tsRelease, smExternal);

  // return always true
  return true;
}

bool cSatipTuner::Connect(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  if (!isempty(*streamAddrM)) {
     cString connectionUri = cString::sprintf("rtsp://%s/", *streamAddrM);
     // Just retune
     if (streamIdM >= 0) {
        cString uri = cString::sprintf("%sstream=%d?%s", *connectionUri, streamIdM, *streamParamM);
        debug("%s Retuning [device %d]", __PRETTY_FUNCTION__, deviceIdM);
        if (rtspM.Play(*uri)) {
           keepAliveM.Set(timeoutM);
           return true;
           }
        }
     else if (rtspM.Options(*connectionUri)) {
        cString uri = cString::sprintf("%s?%s", *connectionUri, *streamParamM);
        // Flush any old content
        rtpM.Flush();
        rtcpM.Flush();
        if (nextServerM && nextServerM->Quirk(cSatipServer::eSatipQuirkSessionId))
           rtspM.SetSession(SkipZeroes(*sessionM));
        if (rtspM.Setup(*uri, rtpM.Port(), rtcpM.Port())) {
           keepAliveM.Set(timeoutM);
           if (nextServerM) {
              cSatipDiscover::GetInstance()->UseServer(nextServerM, true);
              currentServerM = nextServerM;
              nextServerM = NULL;
              }
           return true;
           }
        }
     streamIdM = -1;
     error("Connect failed [device %d]", deviceIdM);
     }

  return false;
}

bool cSatipTuner::Disconnect(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);

  if (!isempty(*streamAddrM) && (streamIdM >= 0)) {
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
     rtspM.Teardown(*uri);
     streamIdM = -1;
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

void cSatipTuner::ProcessVideoData(u_char *bufferP, int lengthP)
{
  //debug("%s(, %d) [device %d]", __PRETTY_FUNCTION__, lengthP, deviceIdM);
  if (lengthP > 0) {
     AddTunerStatistic(lengthP);
     deviceM->WriteData(bufferP, lengthP);
     }
  reConnectM.Set(eConnectTimeoutMs);
}

void cSatipTuner::ProcessApplicationData(u_char *bufferP, int lengthP)
{
  //debug("%s(%d) [device %d]", __PRETTY_FUNCTION__, lengthP, deviceIdM);
  // DVB-S2:
  // ver=<major>.<minor>;src=<srcID>;tuner=<feID>,<level>,<lock>,<quality>,<frequency>,<polarisation>,<system>,<type>,<pilots>,<roll_off>,<symbol_rate>,<fec_inner>;pids=<pid0>,...,<pidn>
  // DVB-T2:
  // ver=1.1;tuner=<feID>,<level>,<lock>,<quality>,<freq>,<bw>,<msys>,<tmode>,<mtype>,<gi>,<fec>,<plp>,<t2id>,<sm>;pids=<pid0>,...,<pidn>
  // DVB-C2:
  // ver=1.2;tuner=<feID>,<level>,<lock>,<quality>,<freq>,<bw>,<msys>,<mtype>,<sr>,<c2tft>,<ds>,<plp>,<specinv>;pids=<pid0>,...,<pidn>
  if (lengthP > 0) {
     char s[lengthP];
     memcpy(s, (char *)bufferP, lengthP);
     //debug("%s(%s) [device %d]", __PRETTY_FUNCTION__, s, deviceIdM);
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
     }
  reConnectM.Set(eConnectTimeoutMs);
}

void cSatipTuner::SetStreamId(int streamIdP)
{
  cMutexLock MutexLock(&mutexM);
  debug("%s(%d) [device %d]", __PRETTY_FUNCTION__, streamIdP, deviceIdM);
  streamIdM = streamIdP;
}

void cSatipTuner::SetSessionTimeout(const char *sessionP, int timeoutP)
{
  cMutexLock MutexLock(&mutexM);
  debug("%s(%s, %d) [device %d]", __PRETTY_FUNCTION__, sessionP, timeoutP, deviceIdM);
  sessionM = sessionP;
  timeoutM = (timeoutP > eMinKeepAliveIntervalMs) ? timeoutP : eMinKeepAliveIntervalMs;
}

int cSatipTuner::GetId(void)
{
  //debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return deviceIdM;
}

bool cSatipTuner::SetSource(cSatipServer *serverP, const char *parameterP, const int indexP)
{
  debug("%s(%s, %d) [device %d]", __PRETTY_FUNCTION__, parameterP, indexP, deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (serverP) {
     nextServerM = cSatipDiscover::GetInstance()->GetServer(serverP);
     if (nextServerM && !isempty(nextServerM->Address()) && !isempty(parameterP)) {
        // Update stream address and parameter
        streamAddrM = rtspM.RtspUnescapeString(nextServerM->Address());
        streamParamM = rtspM.RtspUnescapeString(parameterP);
        // Reconnect
        RequestState(tsSet, smExternal);
        }
     }
  else {
     streamAddrM = "";
     streamParamM = "";
     }

  return true;
}

bool cSatipTuner::SetPid(int pidP, int typeP, bool onP)
{
  //debug("%s(%d, %d, %d) [device %d]", __PRETTY_FUNCTION__, pidP, typeP, onP, deviceIdM);
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
  sleepM.Signal();

  return true;
}

bool cSatipTuner::UpdatePids(bool forceP)
{
  //debug("%s(%d) tunerState=%s [device %d]", __PRETTY_FUNCTION__, forceP, TunerStateString(currentStateM), deviceIdM);
  if (((forceP && pidsM.Size()) || (pidUpdateCacheM.TimedOut() && (addPidsM.Size() || delPidsM.Size()))) &&
      !isempty(*streamAddrM) && (streamIdM > 0)) {
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
     if (!rtspM.Play(*uri))
        return false;
     addPidsM.Clear();
     delPidsM.Clear();
     }

  return true;
}

bool cSatipTuner::KeepAlive(bool forceP)
{
  //debug("%s(%d) tunerState=%s [device %d]", __PRETTY_FUNCTION__, forceP, TunerStateString(currentStateM), deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (keepAliveM.TimedOut()) {
     keepAliveM.Set(timeoutM);
     forceP = true;
     }
  if (forceP && !isempty(*streamAddrM)) {
     cString uri = cString::sprintf("rtsp://%s/", *streamAddrM);
     if (!rtspM.Options(*uri))
        return false;
     }

  return true;
}

bool cSatipTuner::ReadReceptionStatus(bool forceP)
{
  //debug("%s(%d) tunerState=%s [device %d]", __PRETTY_FUNCTION__, forceP, TunerStateString(currentStateM), deviceIdM);
  cMutexLock MutexLock(&mutexM);
  if (statusUpdateM.TimedOut()) {
     statusUpdateM.Set(eStatusUpdateTimeoutMs);
     forceP = true;
     }
  if (forceP && !isempty(*streamAddrM) && (streamIdM > 0)) {
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
     if (rtspM.Describe(*uri))
        return true;
     }

  return false;
}

void cSatipTuner::UpdateCurrentState(void)
{
  //debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  cMutexLock MutexLock(&mutexM);
  eTunerState state = currentStateM;

  if (internalStateM.Size()) {
     state = internalStateM.At(0);
     internalStateM.Remove(0);
     }
  else if (externalStateM.Size()) {
     state = externalStateM.At(0);
     externalStateM.Remove(0);
     }

  if (currentStateM != state) {
     debug("%s: switching from %s to %s [device %d]", __PRETTY_FUNCTION__, TunerStateString(currentStateM), TunerStateString(state), deviceIdM);
     currentStateM = state;
     }
}

bool cSatipTuner::StateRequested(void)
{
  cMutexLock MutexLock(&mutexM);
  //debug("%s current=%s internal=%d external=%d [device %d]", __PRETTY_FUNCTION__, TunerStateString(currentStateM), internalStateM.Size(), externalStateM.Size(), deviceIdM);

  return (internalStateM.Size() || externalStateM.Size());
}

bool cSatipTuner::RequestState(eTunerState stateP, eStateMode modeP)
{
  cMutexLock MutexLock(&mutexM);
  debug("%s(%s, %s) current=%s internal=%d external=%d [device %d]", __PRETTY_FUNCTION__, TunerStateString(stateP), StateModeString(modeP), TunerStateString(currentStateM), internalStateM.Size(), externalStateM.Size(), deviceIdM);

  if (modeP == smExternal)
     externalStateM.Append(stateP);
  else if (modeP == smInternal) {
     eTunerState state = internalStateM.Size() ? internalStateM.At(internalStateM.Size() - 1) : currentStateM;

     // validate legal state changes
     switch (state) {
       case tsIdle:
            if (stateP == tsRelease)
               return false;
       case tsRelease:
       case tsSet:
       case tsLocked:
       case tsTuned:
       default:
            break;
       }

     internalStateM.Append(stateP);
     }
  else
     return false;

  return true;
}

const char *cSatipTuner::StateModeString(eStateMode modeP)
{
  switch (modeP) {
    case smInternal:
         return "smInternal";
    case smExternal:
         return "smExternal";
     default:
         break;
    }

   return "---";
}

const char *cSatipTuner::TunerStateString(eTunerState stateP)
{
  switch (stateP) {
    case tsIdle:
         return "tsIdle";
    case tsRelease:
         return "tsRelease";
    case tsSet:
         return "tsSet";
    case tsLocked:
         return "tsLocked";
    case tsTuned:
         return "tsTuned";
    default:
         break;
    }

  return "---";
}

int cSatipTuner::SignalStrength(void)
{
  //debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return signalStrengthM;
}

int cSatipTuner::SignalQuality(void)
{
  //debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return signalQualityM;
}

bool cSatipTuner::HasLock(void)
{
  //debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return (currentStateM >= tsTuned) && hasLockM;
}

cString cSatipTuner::GetSignalStatus(void)
{
  //debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return cString::sprintf("lock=%d strength=%d quality=%d", HasLock(), SignalStrength(), SignalQuality());
}

cString cSatipTuner::GetInformation(void)
{
  //debug("%s [device %d]", __PRETTY_FUNCTION__, deviceIdM);
  return (currentStateM >= tsTuned) ? cString::sprintf("rtsp://%s/?%s [stream=%d]", *streamAddrM, *streamParamM, streamIdM) : "connection failed";
}
