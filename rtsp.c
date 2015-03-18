/*
 * rtsp.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>

#include "config.h"
#include "common.h"
#include "log.h"
#include "rtsp.h"

cSatipRtsp::cSatipRtsp(cSatipTunerIf &tunerP)
: tunerM(tunerP),
  modeM(cmUnicast),
  handleM(NULL),
  headerListM(NULL),
  errorNoMore(""),
  errorOutOfRange(""),
  errorCheckSyntax("")
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  Create();
}

cSatipRtsp::~cSatipRtsp()
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  Destroy();
}

size_t cSatipRtsp::HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(dataP);
  size_t len = sizeP * nmembP;
  debug16("%s len=%zu", __PRETTY_FUNCTION__, len);

  char *s, *p = (char *)ptrP;
  char *r = strtok_r(p, "\r\n", &s);

  while (obj && r) {
        debug16("%s (%zu): %s", __PRETTY_FUNCTION__, len, r);
        r = skipspace(r);
        if (strstr(r, "com.ses.streamID")) {
           int streamid = -1;
           if (sscanf(r, "com.ses.streamID:%11d", &streamid) == 1)
              obj->tunerM.SetStreamId(streamid);
           }
        else if (strstr(r, "Session:")) {
           int timeout = -1;
           char *session = NULL;
           if (sscanf(r, "Session:%m[^;];timeout=%11d", &session, &timeout) == 2)
              obj->tunerM.SetSessionTimeout(skipspace(session), timeout * 1000);
           else if (sscanf(r, "Session:%m[^;]", &session) == 1)
              obj->tunerM.SetSessionTimeout(skipspace(session), -1);
           FREE_POINTER(session);
           }
        r = strtok_r(NULL, "\r\n", &s);
        }

  return len;
}

size_t cSatipRtsp::DescribeCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(dataP);
  size_t len = sizeP * nmembP;
  debug16("%s len=%zu", __PRETTY_FUNCTION__, len);

  if (obj && (len > 0))
     obj->tunerM.ProcessApplicationData((u_char*)ptrP, len);

  return len;
}

size_t cSatipRtsp::SetupPlayCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(dataP);
  size_t len = sizeP * nmembP;
  debug16("%s len=%zu", __PRETTY_FUNCTION__, len);

  char *s, *p = (char *)ptrP;
  char *r = strtok_r(p, "\r\n", &s);

  while (obj && r) {
        debug16("%s (%zu): %s", __PRETTY_FUNCTION__, len, r);
        r = skipspace(r);
        if (strstr(r, "No-More:")) {
           char *tmp = NULL;
           if (sscanf(r, "No-More:%m[^;]", &tmp) == 1)
              obj->SetErrorNoMore(skipspace(tmp));
           FREE_POINTER(tmp);
           }
        else if (strstr(r, "Out-of-Range:")) {
           char *tmp = NULL;
           if (sscanf(r, "Out-of-Range:%m[^;]", &tmp) == 1)
              obj->SetErrorOutOfRange(skipspace(tmp));
           FREE_POINTER(tmp);
           }
        else if (strstr(r, "Check-Syntax:")) {
           char *tmp = NULL;
           if (sscanf(r, "Check-Syntax:%m[^;]", &tmp) == 1)
              obj->SetErrorCheckSyntax(skipspace(tmp));
           FREE_POINTER(tmp);
           }
        r = strtok_r(NULL, "\r\n", &s);
        }

  return len;
}

int cSatipRtsp::DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(userPtrP);

  if (obj) {
     switch (typeP) {
       case CURLINFO_TEXT:
            debug2("%s [device %d] RTSP INFO %.*s", __PRETTY_FUNCTION__, obj->tunerM.GetId(), (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_IN:
            debug2("%s [device %d] RTSP HEAD <<< %.*s", __PRETTY_FUNCTION__, obj->tunerM.GetId(), (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_OUT:
            debug2("%s [device %d] RTSP HEAD >>>\n%.*s", __PRETTY_FUNCTION__, obj->tunerM.GetId(), (int)sizeP, dataP);
            break;
       case CURLINFO_DATA_IN:
            debug2("%s [device %d] RTSP DATA <<< %.*s", __PRETTY_FUNCTION__, obj->tunerM.GetId(), (int)sizeP, dataP);
            break;
       case CURLINFO_DATA_OUT:
            debug2("%s [device %d] RTSP DATA >>>\n%.*s", __PRETTY_FUNCTION__, obj->tunerM.GetId(), (int)sizeP, dataP);
            break;
       default:
            break;
       }
     }

  return 0;
}

cString cSatipRtsp::RtspUnescapeString(const char *strP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, strP, tunerM.GetId());
  if (handleM) {
     char *p = curl_easy_unescape(handleM, strP, 0, NULL);
     cString s = p;
     curl_free(p);

     return s;
     }

  return cString(strP);
}

void cSatipRtsp::Create(void)
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  if (!handleM)
     handleM = curl_easy_init();

  if (handleM) {
     CURLcode res = CURLE_OK;

     // Verbose output
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_VERBOSE, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_DEBUGFUNCTION, cSatipRtsp::DebugCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_DEBUGDATA, this);

     // No progress meter and no signaling
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOPROGRESS, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOSIGNAL, 1L);

     // Set timeouts
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_TIMEOUT_MS, (long)eConnectTimeoutMs);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_CONNECTTIMEOUT_MS, (long)eConnectTimeoutMs);

     // Set user-agent
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s (device %d)", PLUGIN_NAME_I18N, VERSION, tunerM.GetId()));
     }
}

void cSatipRtsp::Destroy(void)
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
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

void cSatipRtsp::Reset(void)
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  Destroy();
  Create();
}

bool cSatipRtsp::Options(const char *uriP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, uriP, tunerM.GetId());
  bool result = false;

  if (handleM && !isempty(uriP)) {
     long rc = 0;
     cTimeMs processing(0);
     CURLcode res = CURLE_OK;

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_URL, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
     SATIP_CURL_EASY_PERFORM(handleM);

     result = ValidateLatestResponse(&rc);
     debug5("%s (%s) Response %ld in %" PRIu64 " ms [device %d]", __PRETTY_FUNCTION__, uriP, rc, processing.Elapsed(), tunerM.GetId());
     }

  return result;
}

bool cSatipRtsp::Setup(const char *uriP, int rtpPortP, int rtcpPortP)
{
  debug1("%s (%s, %d, %d) [device %d]", __PRETTY_FUNCTION__, uriP, rtpPortP, rtcpPortP, tunerM.GetId());
  bool result = false;

  if (handleM && !isempty(uriP)) {
     cString transport;
     long rc = 0;
     cTimeMs processing(0);
     CURLcode res = CURLE_OK;

     switch (modeM) {
       case cmMulticast:
            // RTP/AVP;multicast;destination=<IP multicast address>;port=<RTP port>-<RTCP port>;ttl=<ttl>
            transport = cString::sprintf("RTP/AVP;multicast;port=%d-%d", rtpPortP, rtcpPortP);
            break;
       default:
       case cmUnicast:
            // RTP/AVP;unicast;client_port=<client RTP port>-<client RTCP port>
            transport = cString::sprintf("RTP/AVP;unicast;client_port=%d-%d", rtpPortP, rtcpPortP);
            break;
       }

     // Setup media stream
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_TRANSPORT, *transport);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
     // Set header callback for catching the session and timeout
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_HEADERFUNCTION, cSatipRtsp::HeaderCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEHEADER, this);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipRtsp::SetupPlayCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     // Session id is now known - disable header parsing
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_HEADERFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEHEADER, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);

     result = ValidateLatestResponse(&rc);
     debug5("%s (%s, %d, %d) Response %ld in %" PRIu64 " ms [device %d]", __PRETTY_FUNCTION__, uriP, rtpPortP, rtcpPortP, rc, processing.Elapsed(), tunerM.GetId());
     }

  return result;
}

bool cSatipRtsp::SetSession(const char *sessionP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, sessionP, tunerM.GetId());
  if (handleM) {
     CURLcode res = CURLE_OK;

     debug1("%s: session id quirk enabled [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_SESSION_ID, sessionP);
     }

  return true;
}

bool cSatipRtsp::Describe(const char *uriP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, uriP, tunerM.GetId());
  bool result = false;

  if (handleM && !isempty(uriP)) {
     long rc = 0;
     cTimeMs processing(0);
     CURLcode res = CURLE_OK;

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipRtsp::DescribeCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);

     result = ValidateLatestResponse(&rc);
     debug5("%s (%s) Response %ld in %" PRIu64 " ms [device %d]", __PRETTY_FUNCTION__, uriP, rc, processing.Elapsed(), tunerM.GetId());
     }

  return result;
}

bool cSatipRtsp::Play(const char *uriP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, uriP, tunerM.GetId());
  bool result = false;

  if (handleM && !isempty(uriP)) {
     long rc = 0;
     cTimeMs processing(0);
     CURLcode res = CURLE_OK;

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipRtsp::SetupPlayCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);

     result = ValidateLatestResponse(&rc);
     debug5("%s (%s) Response %ld in %" PRIu64 " ms [device %d]", __PRETTY_FUNCTION__, uriP, rc, processing.Elapsed(), tunerM.GetId());
     }

  return result;
}

bool cSatipRtsp::Teardown(const char *uriP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, uriP, tunerM.GetId());
  bool result = false;

  if (handleM && !isempty(uriP)) {
     long rc = 0;
     cTimeMs processing(0);
     CURLcode res = CURLE_OK;

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_STREAM_URI, uriP);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipRtsp::SetupPlayCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_CLIENT_CSEQ, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_SESSION_ID, NULL);

     result = ValidateLatestResponse(&rc);
     debug5("%s (%s) Response %ld in %" PRIu64 " ms [device %d]", __PRETTY_FUNCTION__, uriP, rc, processing.Elapsed(), tunerM.GetId());
     }

  return result;
}

void cSatipRtsp::SetErrorNoMore(const char *strP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, strP, tunerM.GetId());
  errorNoMore = strP;
}

void cSatipRtsp::SetErrorOutOfRange(const char *strP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, strP, tunerM.GetId());
  errorOutOfRange = strP;
}

void cSatipRtsp::SetErrorCheckSyntax(const char *strP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, strP, tunerM.GetId());
  errorCheckSyntax = strP;
}

bool cSatipRtsp::ValidateLatestResponse(long *rcP)
{
  bool result = false;

  if (handleM) {
     char *url = NULL;
     long rc = 0;
     CURLcode res = CURLE_OK;
     SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_RESPONSE_CODE, &rc);
     switch (rc) {
       case 200:
            result = true;
            break;
       case 400:
            // SETUP PLAY TEARDOWN
            // The message body of the response may contain the "Check-Syntax:" parameter followed
            // by the malformed syntax
            if (!isempty(*errorCheckSyntax)) {
               SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_EFFECTIVE_URL, &url);
               error("Check syntax: %s (error code %ld: %s) [device %d]", *errorCheckSyntax, rc, url, tunerM.GetId());
               break;
               }
       case 403:
            // SETUP PLAY TEARDOWN
            // The message body of the response may contain the "Out-of-Range:" parameter followed
            // by a space-separated list of the attribute names that are not understood:
            // "src" "fe" "freq" "pol" "msys" "mtype" "plts" "ro" "sr" "fec" "pids" "addpids" "delpids" "mcast
            if (!isempty(*errorOutOfRange)) {
               SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_EFFECTIVE_URL, &url);
               error("Out of range: %s (error code %ld: %s) [device %d]", *errorOutOfRange, rc, url, tunerM.GetId());
               break;
               }
       case 503:
            // SETUP PLAY
            // The message body of the response may contain the "No-More:" parameter followed
            // by a space-separated list of the missing ressources: “sessions” "frontends" "pids
            if (!isempty(*errorNoMore)) {
               SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_EFFECTIVE_URL, &url);
               error("No more: %s (error code %ld: %s) [device %d]", *errorNoMore, rc, url, tunerM.GetId());
               break;
               }
       default:
            SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_EFFECTIVE_URL, &url);
            error("Detected invalid status code %ld: %s [device %d]", rc, url, tunerM.GetId());
            break;
       }
     if (rcP)
        *rcP = rc;
     }
  errorNoMore = "";
  errorOutOfRange = "";
  errorCheckSyntax = "";
  debug1("%s result=%s [device %d]", __PRETTY_FUNCTION__, result ? "ok" : "failed", tunerM.GetId());

  return result;
}
