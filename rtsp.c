/*
 * rtsp.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "rtsp.h"

cSatipRtsp::cSatipRtsp(cSatipTunerIf &tunerP)
: tunerM(&tunerP),
  tunerIdM(tunerM ? tunerM->GetId() : -1),
  handleM(curl_easy_init()),
  headerListM(NULL)
{
  if (handleM) {
     CURLcode res = CURLE_OK;

#ifdef DEBUG
     // Verbose output
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_VERBOSE, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_DEBUGFUNCTION, cSatipRtsp::DebugCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_DEBUGDATA, this);
#endif

     // No progress meter and no signaling
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOPROGRESS, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOSIGNAL, 1L);

     // Set timeouts
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_TIMEOUT_MS, (long)eConnectTimeoutMs);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_CONNECTTIMEOUT_MS, (long)eConnectTimeoutMs);

     // Set user-agent
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s (device %d)", PLUGIN_NAME_I18N, VERSION, tunerIdM));
     }
}

cSatipRtsp::~cSatipRtsp()
{
  if (handleM) {
     // Cleanup curl stuff
     if (headerListM) {
        curl_slist_free_all(headerListM);
        headerListM = NULL;
        }
     curl_easy_cleanup(handleM);
     handleM = NULL;
     }
}

size_t cSatipRtsp::HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(dataP);
  size_t len = sizeP * nmembP;
  //debug("cSatipRtsp::%s(%zu)", __FUNCTION__, len);

  char *s, *p = (char *)ptrP;
  char *r = strtok_r(p, "\r\n", &s);

  while (obj && obj->tunerM && r) {
        //debug("cSatipRtsp::%s(%zu): %s", __FUNCTION__, len, r);
        r = skipspace(r);
        if (strstr(r, "com.ses.streamID")) {
           int streamid = -1;
           if (sscanf(r, "com.ses.streamID:%11d", &streamid) == 1)
              obj->tunerM->SetStreamId(streamid);
           }
        else if (strstr(r, "Session:")) {
           int timeout = -1;
           char *session = NULL;
           if (sscanf(r, "Session:%m[^;];timeout=%11d", &session, &timeout) == 2)
              obj->tunerM->SetSessionTimeout(skipspace(session), timeout * 1000);
           else if (sscanf(r, "Session:%m[^;]", &session) == 1)
              obj->tunerM->SetSessionTimeout(skipspace(session), -1);
           FREE_POINTER(session);
           }
        r = strtok_r(NULL, "\r\n", &s);
        }

  return len;
}

size_t cSatipRtsp::WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(dataP);
  size_t len = sizeP * nmembP;
  //debug("cSatipRtsp::%s(%zu)", __FUNCTION__, len);

  if (obj && obj->tunerM && (len > 0)) {
     char *data = strndup((char*)ptrP, len);
     obj->tunerM->ParseReceptionParameters(data);
     FREE_POINTER(data);
     }

  return len;
}

int cSatipRtsp::DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(userPtrP);

  if (obj && obj->tunerM) {
     switch (typeP) {
       case CURLINFO_TEXT:
            debug("cSatipTuner::%s(%d): RTSP INFO %.*s", __FUNCTION__, obj->tunerM->GetId(), (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_IN:
            debug("cSatipTuner::%s(%d): RTSP HEAD <<< %.*s", __FUNCTION__, obj->tunerM->GetId(), (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_OUT:
            debug("cSatipTuner::%s(%d): RTSP HEAD >>>\n%.*s", __FUNCTION__, obj->tunerM->GetId(), (int)sizeP, dataP);
            break;
       case CURLINFO_DATA_IN:
            debug("cSatipTuner::%s(%d): RTSP DATA <<< %.*s", __FUNCTION__, obj->tunerM->GetId(), (int)sizeP, dataP);
            break;
       case CURLINFO_DATA_OUT:
            debug("cSatipTuner::%s(%d): RTSP DATA >>>\n%.*s", __FUNCTION__, obj->tunerM->GetId(), (int)sizeP, dataP);
            break;
       default:
            break;
       }
     }

  return 0;
}

cString cSatipRtsp::RtspUnescapeString(const char *strP)
{
  debug("cSatipRtsp::%s(%d): str=%s", __FUNCTION__, tunerIdM, strP);
  if (handleM) {
     char *p = curl_easy_unescape(handleM, strP, 0, NULL);
     cString s = p;
     curl_free(p);

     return s;
     }

  return cString(strP);
}

bool cSatipRtsp::Options(const char *uriP)
{
  debug("cSatipRtsp::%s(%d): uri=%s", __FUNCTION__, tunerIdM, uriP);
  if (handleM && !isempty(uriP)) {
     CURLcode res = CURLE_OK;

     if (!strstr(uriP, "?"))
        SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_URL, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
     SATIP_CURL_EASY_PERFORM(handleM);

     return ValidateLatestResponse();
     }

  return false;
}

bool cSatipRtsp::Setup(const char *uriP, int rtpPortP, int rtcpPortP)
{
  debug("cSatipRtsp::%s(%d): uri=%s rtp=%d rtcp=%d", __FUNCTION__, tunerIdM, uriP, rtpPortP, rtcpPortP);
  if (handleM && !isempty(uriP)) {
     CURLcode res = CURLE_OK;
     cString transport = cString::sprintf("RTP/AVP;unicast;client_port=%d-%d", rtpPortP, rtcpPortP);

     // Setup media stream
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_TRANSPORT, *transport);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
     // Set header callback for catching the session and timeout
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_HEADERFUNCTION, cSatipRtsp::HeaderCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEHEADER, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     // Session id is now known - disable header parsing
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_HEADERFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEHEADER, NULL);

     return ValidateLatestResponse();
     }

  return false;
}

bool cSatipRtsp::SetSession(const char *sessionP)
{
  debug("cSatipRtsp::%s(%d): session=%s", __FUNCTION__, tunerIdM, sessionP);
  if (handleM) {
     CURLcode res = CURLE_OK;

     debug("cSatipRtsp::%s(%d): session id quirk enabled", __FUNCTION__, tunerIdM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_SESSION_ID, sessionP);
     }

  return true;
}

bool cSatipRtsp::Describe(const char *uriP)
{
  debug("cSatipRtsp::%s(%d): uri=%s", __FUNCTION__, tunerIdM, uriP);
  if (handleM && !isempty(uriP)) {
     CURLcode res = CURLE_OK;

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipRtsp::WriteCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);

     return ValidateLatestResponse();
     }

  return false;
}

bool cSatipRtsp::Play(const char *uriP)
{
  debug("cSatipRtsp::%s(%d): uri=%s", __FUNCTION__, tunerIdM, uriP);
  if (handleM && !isempty(uriP)) {
     CURLcode res = CURLE_OK;

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
     SATIP_CURL_EASY_PERFORM(handleM);

     return ValidateLatestResponse();
     }

  return false;
}

bool cSatipRtsp::Teardown(const char *uriP)
{
  debug("cSatipRtsp::%s(%d): uri=%s", __FUNCTION__, tunerIdM, uriP);
  if (handleM && !isempty(uriP)) {
     CURLcode res = CURLE_OK;

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN);
     SATIP_CURL_EASY_PERFORM(handleM);

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_CLIENT_CSEQ, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_SESSION_ID, NULL);

     return ValidateLatestResponse();
     }

  return false;
}

bool cSatipRtsp::ValidateLatestResponse(void)
{
  debug("cSatipRtsp::%s(%d)", __FUNCTION__, tunerIdM);
  if (handleM) {
     long rc = 0;
     CURLcode res = CURLE_OK;
     SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_RESPONSE_CODE, &rc);
     if (rc == 200)
        return true;
     else if (rc != 0)
        error("Tuner detected invalid status code %ld [device %d]", rc, tunerIdM);
     }

  return false;
}
