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
  sleepM(),
  deviceM(&deviceP),
  packetBufferLenM(packetLenP),
  rtpSocketM(new cSatipSocket()),
  rtcpSocketM(new cSatipSocket()),
  streamAddrM(""),
  streamParamM(""),
  currentServerM(NULL),
  nextServerM(NULL),
  mutexM(),
  handleM(NULL),
  headerListM(NULL),
  keepAliveM(),
  pidUpdateCacheM(),
  sessionM(),
  timeoutM(eKeepAliveIntervalMs),
  openedM(false),
  tunedM(false),
  hasLockM(false),
  signalStrengthM(-1),
  signalQualityM(-1),
  streamIdM(-1),
  pidUpdatedM(false),
  pidsM()
{
  debug("cSatipTuner::%s(%d)", __FUNCTION__, packetBufferLenM);
  // Allocate packet buffer
  packetBufferM = MALLOC(unsigned char, packetBufferLenM);
  if (packetBufferM)
     memset(packetBufferM, 0, packetBufferLenM);
  else
     error("MALLOC() failed for packet buffer");
  // Start thread
  Start();
}

cSatipTuner::~cSatipTuner()
{
  debug("cSatipTuner::%s()", __FUNCTION__);
  // Stop thread
  sleepM.Signal();
  if (Running())
     Cancel(3);
  Close();
  // Free allocated memory
  free(packetBufferM);
  DELETENULL(rtcpSocketM);
  DELETENULL(rtpSocketM);
}

size_t cSatipTuner::HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipTuner *obj = reinterpret_cast<cSatipTuner *>(dataP);
  size_t len = sizeP * nmembP;
  //debug("cSatipTuner::%s(%zu)", __FUNCTION__, len);

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
              obj->SetSessionTimeout(skipspace(session), timeout);
           else if (sscanf(r, "Session:%m[^;]", &session) == 1)
              obj->SetSessionTimeout(skipspace(session), -1);
           FREE_POINTER(session);
           }
        r = strtok_r(NULL, "\r\n", &s);
        }

  return len;
}

void cSatipTuner::Action(void)
{
  debug("cSatipTuner::%s(): entering", __FUNCTION__);
  cTimeMs timeout(0);
  // Increase priority
  SetPriority(-1);
  // Do the thread loop
  while (packetBufferM && Running()) {
        int length = -1;
        unsigned int size = min(deviceM->CheckData(), packetBufferLenM);
        if (tunedM && (size > 0)) {
           // Update pids
           UpdatePids();
           // Remember the heart beat
           KeepAlive();
           // Read reception statistics
           if (rtcpSocketM && rtcpSocketM->IsOpen()) {
              unsigned char buf[1450];
              memset(buf, 0, sizeof(buf));
              if (rtcpSocketM->ReadApplication(buf, sizeof(buf)) > 0) {
                 ParseReceptionParameters((const char *)buf);
                 timeout.Set(eReConnectTimeoutMs);
                 }
              }
           // Read data
           if (rtpSocketM && rtpSocketM->IsOpen())
              length = rtpSocketM->ReadVideo(packetBufferM, size);
           }
        if (length > 0) {
           AddTunerStatistic(length);
           deviceM->WriteData(packetBufferM, length);
           timeout.Set(eReConnectTimeoutMs);
           }
        else {
           // Reconnect if necessary
           if (openedM && !tunedM && timeout.TimedOut()) {
              Disconnect();
              Connect();
              timeout.Set(eReConnectTimeoutMs);
              }
           sleepM.Wait(10); // to avoid busy loop and reduce cpu load
           }
        }
  debug("cSatipTuner::%s(): exiting", __FUNCTION__);
}

bool cSatipTuner::Open(void)
{
  debug("cSatipTuner::%s()", __FUNCTION__);
  if (Connect()) {
     openedM = true;
     return true;
     }
  return false;
}

bool cSatipTuner::Close(void)
{
  debug("cSatipTuner::%s()", __FUNCTION__);
  openedM = false;
  Disconnect();
  return true;
}

bool cSatipTuner::Connect(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s()", __FUNCTION__);

  // Initialize the curl session
  if (!handleM)
     handleM = curl_easy_init();

  if (handleM && !isempty(*streamAddrM)) {
     cString uri, control, transport, range;
     CURLcode res = CURLE_OK;

     // Just retune
     if (tunedM && (streamIdM >= 0)) {
        debug("cSatipTuner::%s(): retune", __FUNCTION__);
        uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
        SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
        SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
        SATIP_CURL_EASY_PERFORM(handleM);
        if (!ValidateLatestResponse())
           return false;
        // Flush any old content
        if (rtpSocketM)
           rtpSocketM->Flush();
        keepAliveM.Set(eKeepAliveIntervalMs);
        return true;
        }

#ifdef DEBUG
     // Verbose output
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_VERBOSE, 1L);
#endif

     // Set callback
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_HEADERFUNCTION, cSatipTuner::HeaderCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEHEADER, this);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);

     // No progress meter and no signaling
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOPROGRESS, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOSIGNAL, 1L);

     // Set timeouts
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_TIMEOUT_MS, (long)eConnectTimeoutMs);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_CONNECTTIMEOUT_MS, (long)eConnectTimeoutMs);

     // Set user-agent
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s", PLUGIN_NAME_I18N, VERSION));

     // Set URL
     char *p = curl_easy_unescape(handleM, *streamAddrM, 0, NULL);
     streamAddrM = p;
     curl_free(p);
     uri = cString::sprintf("rtsp://%s/", *streamAddrM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_URL, *uri);

     // Open sockets
     int i = 100;
     while (i-- > 0) {
           if (rtpSocketM->Open() && rtcpSocketM->Open(rtpSocketM->Port() + 1))
              break;
           rtpSocketM->Close();
           rtcpSocketM->Close();
           }
     if ((rtpSocketM->Port() <= 0) || (rtcpSocketM->Port() <= 0)) {
        error("Cannot open required ports!");
        return false;
        }

     // Request server options: "&pids=all" for the whole mux
     uri = cString::sprintf("rtsp://%s/?%s&pids=0", *streamAddrM, *streamParamM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
     SATIP_CURL_EASY_PERFORM(handleM);
     if (!ValidateLatestResponse())
        return false;

     // Setup media stream
     transport = cString::sprintf("RTP/AVP;unicast;client_port=%d-%d", rtpSocketM->Port(), rtcpSocketM->Port());
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_TRANSPORT, *transport);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
     SATIP_CURL_EASY_PERFORM(handleM);
     if (!ValidateLatestResponse())
        return false;

     // Start playing
     uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_SESSION_ID, *sessionM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
     SATIP_CURL_EASY_PERFORM(handleM);
     if (!ValidateLatestResponse())
        return false;

     keepAliveM.Set(eKeepAliveIntervalMs);
     tunedM = true;
     if (nextServerM) {
        cSatipDiscover::GetInstance()->UseServer(nextServerM, true);
        currentServerM = nextServerM;
        nextServerM = NULL;
        }
     return true;
     }

  return false;
}

bool cSatipTuner::Disconnect(void)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s()", __FUNCTION__);

  // Terminate curl session
  if (handleM) {
     // Teardown rtsp session
     if (!isempty(*streamAddrM) && streamIdM >= 0) {
        CURLcode res = CURLE_OK;

        cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);
        SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
        SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN);
        SATIP_CURL_EASY_PERFORM(handleM);
        ValidateLatestResponse();
        }

     // Cleanup curl stuff
     if (headerListM) {
        curl_slist_free_all(headerListM);
        headerListM = NULL;
        }
     curl_easy_cleanup(handleM);
     handleM = NULL;
     }

  // Close the listening sockets
  rtpSocketM->Close();
  rtcpSocketM->Close();

  // Reset signal parameters
  hasLockM = false;
  signalStrengthM = -1;
  signalQualityM = -1;

  if (currentServerM)
     cSatipDiscover::GetInstance()->UseServer(currentServerM, false);
  tunedM = false;

  return true;
}

bool cSatipTuner::ValidateLatestResponse(void)
{
  //debug("cSatipTuner::%s()", __FUNCTION__);
  if (handleM) {
     long rc = 0;
     CURLcode res = CURLE_OK;
     SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_RESPONSE_CODE, &rc);
     if (rc == 200)
        return true;
     else if (rc != 0)
        error("Tuner detected invalid status code: %ld", rc);
     }

  return false;
}

void cSatipTuner::ParseReceptionParameters(const char *paramP)
{
  //debug("cSatipTuner::%s(%s)", __FUNCTION__, paramP);
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
        hasLockM = atoi(++c);

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
  debug("cSatipTuner::%s(%d)", __FUNCTION__, streamIdP);
  streamIdM = streamIdP;
}

void cSatipTuner::SetSessionTimeout(const char *sessionP, int timeoutP)
{
  cMutexLock MutexLock(&mutexM);
  debug("cSatipTuner::%s(%s, %d)", __FUNCTION__, sessionP, timeoutP);
  sessionM = sessionP;
  timeoutM = timeoutP > 0 ? timeoutP * 1000L : eKeepAliveIntervalMs;
}

bool cSatipTuner::SetSource(cSatipServer *serverP, const char *parameterP, const int indexP)
{
  debug("cSatipTuner::%s(%s, %d)", __FUNCTION__, parameterP, indexP);
  cMutexLock MutexLock(&mutexM);
  nextServerM = cSatipDiscover::GetInstance()->GetServer(serverP);
  if (nextServerM && !isempty(nextServerM->Address()) && !isempty(parameterP)) {
     // Update stream address and parameter
     streamAddrM = nextServerM->Address();
     streamParamM = parameterP;
     // Reconnect
     Connect();
     }
  return true;
}

bool cSatipTuner::SetPid(int pidP, int typeP, bool onP)
{
  //debug("cSatipTuner::%s(%d, %d, %d)", __FUNCTION__, pidP, typeP, onP);
  cMutexLock MutexLock(&mutexM);
  bool found = false;
  for (int i = 0; i < pidsM.Size(); ++i) {
      if (pidsM[i] == pidP) {
         found = true;
         if (!onP) {
            pidsM.Remove(i);
            pidUpdatedM = true;
            }
         break;
         }
      }
  if (onP && !found) {
      pidsM.Append(pidP);
      pidUpdatedM = true;
      }
  pidUpdateCacheM.Set(ePidUpdateIntervalMs);
  return true;
}

bool cSatipTuner::UpdatePids(void)
{
  cMutexLock MutexLock(&mutexM);
  if (pidUpdateCacheM.TimedOut() && pidUpdatedM && pidsM.Size() && tunedM && handleM && !isempty(*streamAddrM) && (streamIdM > 0)) {
     //debug("cSatipTuner::%s()", __FUNCTION__);
     CURLcode res = CURLE_OK;
     //cString uri = cString::sprintf("rtsp://%s/stream=%d?%spids=%d", *streamAddrM, streamIdM, onP ? "add" : "del", pidP);
     cString uri = cString::sprintf("rtsp://%s/stream=%d?pids=", *streamAddrM, streamIdM);

     for (int i = 0; i < pidsM.Size(); ++i)
         uri = cString::sprintf("%s%d%s", *uri, pidsM[i], (i == (pidsM.Size() - 1)) ? "" : ",");

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
     SATIP_CURL_EASY_PERFORM(handleM);
     if (ValidateLatestResponse()) {
        keepAliveM.Set(eKeepAliveIntervalMs);
        pidUpdatedM = false;
        }
     else
        Disconnect();

     return true;
     }

  return false;
}

bool cSatipTuner::KeepAlive(void)
{
  cMutexLock MutexLock(&mutexM);
  if (tunedM && handleM && keepAliveM.TimedOut()) {
     debug("cSatipTuner::%s()", __FUNCTION__);
     CURLcode res = CURLE_OK;
     cString uri = cString::sprintf("rtsp://%s/stream=%d", *streamAddrM, streamIdM);

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, *uri);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
     SATIP_CURL_EASY_PERFORM(handleM);
     if (ValidateLatestResponse())
        keepAliveM.Set(eKeepAliveIntervalMs);
     else
        Disconnect();

     return true;
     }

  return false;
}

int cSatipTuner::SignalStrength(void)
{
  //debug("cSatipTuner::%s()", __FUNCTION__);
  return signalStrengthM;
}

int cSatipTuner::SignalQuality(void)
{
  //debug("cSatipTuner::%s()", __FUNCTION__);
  return signalQualityM;
}

bool cSatipTuner::HasLock(void)
{
  //debug("cSatipTuner::%s()", __FUNCTION__);
  return tunedM && hasLockM;
}

cString cSatipTuner::GetSignalStatus(void)
{
  //debug("cSatipTuner::%s()", __FUNCTION__);
  return cString::sprintf("lock=%d strength=%d quality=%d", HasLock(), SignalStrength(), SignalQuality());
}

cString cSatipTuner::GetInformation(void)
{
  //debug("cSatipTuner::%s()", __FUNCTION__);
  return tunedM ? cString::sprintf("rtsp://%s/?%s [stream=%d]", *streamAddrM, *streamParamM, streamIdM) : "connection failed";
}
