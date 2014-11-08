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
: cThread("SAT>IP tuner"),
  dataThreadM(deviceP, *this, packetLenP),
  sleepM(),
  deviceM(&deviceP),
  rtpSocketM(new cSatipSocket()),
  rtcpSocketM(new cSatipSocket()),
  streamAddrM(""),
  streamParamM(""),
  currentServerM(NULL),
  nextServerM(NULL),
  mutexM(),
  handleM(NULL),
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
  debug("cSatipTuner::%s(%d) [device %d]", __FUNCTION__, packetLenP, deviceM->GetId());

  RtspInitialize();

  // Open sockets
  for (int i = 100; i > 0; --i) {
      if (rtpSocketM->Open() && rtcpSocketM->Open(rtpSocketM->Port() + 1))
         break;
      rtpSocketM->Close();
      rtcpSocketM->Close();
      }
  if ((rtpSocketM->Port() <= 0) || (rtcpSocketM->Port() <= 0)) {
     error("Cannot open required RTP/RTCP ports [device %d]", deviceM->GetId());
     }
  else {
     info("Using RTP/RTCP ports %d-%d [device %d]", rtpSocketM->Port(), rtcpSocketM->Port(), deviceM->GetId());
     }

  // Start threads
  dataThreadM.Start(rtpSocketM);
  Start();
}

cSatipTuner::~cSatipTuner()
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  // Stop threads
  dataThreadM.Cancel(3);
  sleepM.Signal();
  if (Running())
     Cancel(3);
  Close();

  // Terminate curl session
  RtspTerminate();

  // Close the listening sockets
  if (rtpSocketM)
     rtpSocketM->Close();
  if (rtcpSocketM)
     rtcpSocketM->Close();

  // Free allocated memory
  DELETENULL(rtcpSocketM);
  DELETENULL(rtpSocketM);
}

size_t cSatipTuner::HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipTuner *obj = reinterpret_cast<cSatipTuner *>(dataP);
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, obj->deviceM->GetId());

  size_t len = sizeP * nmembP;

  char *s, *p = (char *)ptrP;
  char *r = strtok_r(p, "\r\n", &s);
  while (obj && r) {
        //debug("cSatipTuner::%s(%zu): %s", __FUNCTION__, len, r);
        r = skipspace(r);
        if (strstr(r, "com.ses.streamID")) {
           int streamid = -1;
           if (sscanf(r, "com.ses.streamID:%11d", &streamid) == 1)
              obj->SetStreamId(streamid);
           }
        else if (strstr(r, "Session:")) {
           int timeout = -1;
           char *session = NULL;
           if (sscanf(r, "Session:%m[^;];timeout=%11d", &session, &timeout) == 2)
              obj->SetSessionTimeout(skipspace(session), timeout * 1000);
           else if (sscanf(r, "Session:%m[^;]", &session) == 1)
              obj->SetSessionTimeout(skipspace(session));
           FREE_POINTER(session);
           }
        r = strtok_r(NULL, "\r\n", &s);
        }

  return len;
}

size_t cSatipTuner::DataCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipTuner *obj = reinterpret_cast<cSatipTuner *>(dataP);
  size_t len = sizeP * nmembP;
  //debug("cSatipTuner::%s(%zu)", __FUNCTION__, len);

  if (obj && (len > 0)) {
     char *data = strndup((char*)ptrP, len);
     obj->ParseReceptionParameters(data);
     FREE_POINTER(data);
     }

  return len;
}

void cSatipTuner::DataTimeoutCallback(void *objP)
{
  cSatipTuner *obj = reinterpret_cast<cSatipTuner *>(objP);
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, obj->deviceM->GetId());

  if (obj)
     obj->reconnectM = true;
}

int cSatipTuner::RtspDebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP)
{
  //cSatipTuner *obj = reinterpret_cast<cSatipTuner *>(userPtrP);
  //debug("cSatipTuner::%s(%d) [device %d]", __FUNCTION__, (int)typeP, obj->deviceM->GetId());

  switch (typeP) {
    case CURLINFO_TEXT:
         debug("RTSP INFO  %.*s", (int)sizeP, dataP);
         break;
    case CURLINFO_HEADER_IN:
         debug("RTSP HEAD< %.*s", (int)sizeP, dataP);
         break;
    case CURLINFO_HEADER_OUT:
         debug("RTSP HEAD> %.*s", (int)sizeP, dataP);
         break;
    case CURLINFO_DATA_IN:
         debug("RTSP DATA< %.*s", (int)sizeP, dataP);
         break;
    case CURLINFO_DATA_OUT:
         debug("RTSP DATA> %.*s", (int)sizeP, dataP);
         break;
    default:
         break;
    }

  return 0;
}

void cSatipTuner::Action(void)
{
  debug("cSatipTuner::%s(): entering [device %d]", __FUNCTION__, deviceM->GetId());
  cTimeMs rtcpTimeout(eReConnectTimeoutMs);
  // Increase priority
  SetPriority(-1);
  // Do the thread loop
  while (Running()) {
        if (reconnectM) {
           info("SAT>IP Device %d timed out. Reconnecting.", deviceM->GetId());
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
              cMutexLock MutexLock(&mutexM);
              if (RtspDescribe())
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
           if (rtcpTimeout.TimedOut())
              reconnectM = true;
           }
           sleepM.Wait(10); // to avoid busy loop and reduce cpu load
        }
  debug("cSatipTuner::%s(): exiting [device %d]", __FUNCTION__, deviceM->GetId());
}

bool cSatipTuner::Open(void)
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  return Connect();
}

bool cSatipTuner::Close(void)
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  return Disconnect();
}

bool cSatipTuner::Connect(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());

  if (isempty(*streamAddrM)) {
     if (tunedM)
        Disconnect();
     tunedM = false;
     return tunedM;
     }

  // Setup stream
  tunedM = RtspSetup(streamParamM, rtpSocketM->Port(), rtcpSocketM->Port());
  if (!tunedM)
     return tunedM;

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
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());

  // Teardown rtsp session
  if (tunedM) {
     RtspTeardown();
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

bool cSatipTuner::ValidateLatestResponse(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  if (handleM) {
     long rc = 0;
     CURLcode res = CURLE_OK;
     SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_RESPONSE_CODE, &rc);
     if (rc == 200)
        return true;
     else if (rc != 0)
        error("Tuner detected invalid status code %ld [device %d]", rc, deviceM->GetId());
     }

  return false;
}

void cSatipTuner::ParseReceptionParameters(const char *paramP)
{
  //debug("cSatipTuner::%s(%s) [device %d]", __FUNCTION__, paramP, deviceM->GetId());
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
  debug("cSatipTuner::%s(%d) [device %d]", __FUNCTION__, streamIdP, deviceM->GetId());
  streamIdM = streamIdP;
}

void cSatipTuner::SetSessionTimeout(const char *sessionP, int timeoutP)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s(%s, %d) [device %d]", __FUNCTION__, sessionP, timeoutP, deviceM->GetId());
  sessionM = sessionP;
  timeoutM = (timeoutP > eMinKeepAliveIntervalMs) ? timeoutP : eMinKeepAliveIntervalMs;
}

bool cSatipTuner::SetSource(cSatipServer *serverP, const char *parameterP, const int indexP)
{
  debug("cSatipTuner::%s(%s, %d) [device %d]", __FUNCTION__, parameterP, indexP, deviceM->GetId());
  cMutexLock MutexLock(&mutexM);

  if (serverP) {
     nextServerM = cSatipDiscover::GetInstance()->GetServer(serverP);
     if (nextServerM && !isempty(nextServerM->Address()) && !isempty(parameterP)) {
        // Update stream address and parameter
        streamAddrM = nextServerM->Address();
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
  debug("cSatipTuner::%s(%d, %d, %d) [device %d]", __FUNCTION__, pidP, typeP, onP, deviceM->GetId());
  cMutexLock MutexLock(&mutexM);

  if (onP) {
     pidsM.AppendUnique(pidP);
     addPidsM.AppendUnique(pidP);
     delPidsM.RemoveElement(pidP);
     }
  else {
     pidsM.RemoveElement(pidP);
     delPidsM.AppendUnique(pidP);
     addPidsM.RemoveElement(pidP);
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

  if (((forceP && pidsM.Size()) ||
      (pidUpdateCacheM.TimedOut() && (addPidsM.Size() || delPidsM.Size())) ) &&
      tunedM && handleM && !isempty(*streamAddrM)) {
     // Disable RTP Timeout while sending PLAY Command
     dataThreadM.SetTimeout(-1, &DataTimeoutCallback, this);
     if (RtspPlay(*GeneratePidParameter(forceP))) {
        addPidsM.Clear();
        delPidsM.Clear();
        dataThreadM.SetTimeout(eReConnectTimeoutMs, &DataTimeoutCallback, this);
        return true;
        }
     Disconnect();
     }

  return false;
}

bool cSatipTuner::KeepAlive(void)
{
  cMutexLock MutexLock(&mutexM);
  if (tunedM && handleM && keepAliveM.TimedOut()) {
     debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
     if (RtspOptions()) {
        keepAliveM.Set(timeoutM);
        return true;
        }
     Disconnect();
     }

  return false;
}

bool cSatipTuner::RtspInitialize()
{
  // Initialize the curl session
  if (!handleM) {
     debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());

     handleM = curl_easy_init();
     CURLcode res = CURLE_OK;

#ifdef DEBUG
     // Verbose output
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_DEBUGFUNCTION, RtspDebugCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_VERBOSE, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_DEBUGDATA, this);
#endif

     // No progress meter and no signaling
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOPROGRESS, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOSIGNAL, 1L);

     // Set timeouts
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_TIMEOUT_MS, (long)eConnectTimeoutMs);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_CONNECTTIMEOUT_MS, (long)eConnectTimeoutMs);

     // Set user-agent
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s (device %d)", PLUGIN_NAME_I18N, VERSION, deviceM->GetId()));

     return !!handleM;
     }

  return false;
}

bool cSatipTuner::RtspTerminate()
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  if (handleM) {
     curl_easy_cleanup(handleM);
     handleM = NULL;
     }

  return true;
}

bool cSatipTuner::RtspSetup(const char *paramP, int rtpPortP, int rtcpPortP)
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  if (handleM) {
     cString uri;
     CURLcode res = CURLE_OK;

     // set URL, this will not change
     // Note that we are unescaping the adress here and do NOT store it for future use.
     char *p = curl_easy_unescape(handleM, *streamAddrM, 0, NULL);
     cString url = cString::sprintf("rtsp://%s/", p);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_URL, *url);
     curl_free(p);

     if (streamIdM >= 0)
        uri = cString::sprintf("rtsp://%s/stream=%d?%s", *streamAddrM, streamIdM, paramP);
     else
        uri = cString::sprintf("rtsp://%s/?%s", *streamAddrM, paramP);

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
     cString transport = cString::sprintf("RTP/AVP;unicast;client_port=%d-%d", rtpPortP, rtcpPortP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_TRANSPORT, *transport);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
     // Set header callback for catching the session and timeout
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_HEADERFUNCTION, cSatipTuner::HeaderCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEHEADER, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     // Session id is now known - disable header parsing
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_HEADERFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEHEADER, NULL);

     // Validate session id
     if (streamIdM < 0) {
        error("Internal Error: No session id received [device %d]", deviceM->GetId());
        return false;
        }
     // For some SATIP boxes e.g. GSSBOX and Triax TSS 400 we need to strip the
     // leading '0' of the sessionID.
     if (nextServerM && nextServerM->Quirk(cSatipServer::eSatipQuirkSessionId) && !isempty(*sessionM) && startswith(*sessionM, "0")) {
        debug("cSatipTuner::%s(): session id quirk [device %d]", __FUNCTION__, deviceM->GetId());
        SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_SESSION_ID, SkipZeroes(*sessionM));
        }

     return ValidateLatestResponse();
     }

  return false;
}

bool cSatipTuner::RtspTeardown(void)
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  if (handleM) {
     CURLcode res = CURLE_OK;
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN);
     SATIP_CURL_EASY_PERFORM(handleM);

     // Reset data we have about the session
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_SESSION_ID, NULL);
     streamIdM = -1;
     sessionM = "";
     timeoutM = eMinKeepAliveIntervalMs;

     return ValidateLatestResponse();
     }

  return false;
}

bool cSatipTuner::RtspPlay(const char *paramP)
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  if (handleM) {
     cString uri;
     CURLcode res = CURLE_OK;

     if (paramP)
        uri = cString::sprintf("rtsp://%s/stream=%d?%s", *streamAddrM, streamIdM, paramP);
     else
        uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
     SATIP_CURL_EASY_PERFORM(handleM);

     return ValidateLatestResponse();
     }

  return false;
}

bool cSatipTuner::RtspOptions(void)
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  if (handleM) {
     CURLcode res = CURLE_OK;
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
     SATIP_CURL_EASY_PERFORM(handleM);

     return ValidateLatestResponse();
     }

  return false;
}

bool cSatipTuner::RtspDescribe(void)
{
  debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  if (handleM) {
     CURLcode res = CURLE_OK;
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipTuner::DataCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);

     if (ValidateLatestResponse())
        return true;

     Disconnect();
     }

  return false;
}

int cSatipTuner::SignalStrength(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  return signalStrengthM;
}

int cSatipTuner::SignalQuality(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  return signalQualityM;
}

bool cSatipTuner::HasLock(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  return tunedM && hasLockM;
}

cString cSatipTuner::GetSignalStatus(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  return cString::sprintf("lock=%d strength=%d quality=%d", HasLock(), SignalStrength(), SignalQuality());
}

cString cSatipTuner::GetInformation(void)
{
  //debug("cSatipTuner::%s() [device %d]", __FUNCTION__, deviceM->GetId());
  return tunedM ? cString::sprintf("rtsp://%s/?%s [stream=%d]", *streamAddrM, *streamParamM, streamIdM) : "connection failed";
}
