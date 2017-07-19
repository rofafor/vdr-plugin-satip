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
  headerBufferM(),
  dataBufferM(),
  handleM(NULL),
  headerListM(NULL),
  errorNoMoreM(""),
  errorOutOfRangeM(""),
  errorCheckSyntaxM(""),
  modeM(cSatipConfig::eTransportModeUnicast),
  interleavedRtpIdM(0),
  interleavedRtcpIdM(1)
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  Create();
}

cSatipRtsp::~cSatipRtsp()
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  Destroy();
}

size_t cSatipRtsp::HeaderCallback(char *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(dataP);
  size_t len = sizeP * nmembP;
  debug16("%s len=%zu", __PRETTY_FUNCTION__, len);

  if (obj && (len > 0))
     obj->headerBufferM.Add(ptrP, len);

  return len;
}

size_t cSatipRtsp::DataCallback(char *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(dataP);
  size_t len = sizeP * nmembP;
  debug16("%s len=%zu", __PRETTY_FUNCTION__, len);

  if (obj)
     obj->dataBufferM.Add(ptrP, len);

  return len;
}

size_t cSatipRtsp::InterleaveCallback(char *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipRtsp *obj = reinterpret_cast<cSatipRtsp *>(dataP);
  size_t len = sizeP * nmembP;
  debug16("%s len=%zu", __PRETTY_FUNCTION__, len);

  if (obj && ptrP && len > 0) {
     char tag = ptrP[0] & 0xFF;
     if (tag == '$') {
        int count = ((ptrP[2] & 0xFF) << 8) | (ptrP[3] & 0xFF);
        if (count > 0) {
           unsigned int channel = ptrP[1] & 0xFF;
           u_char *data = (u_char *)&ptrP[4];
           if (channel == obj->interleavedRtpIdM)
              obj->tunerM.ProcessRtpData(data, count);
           else if (channel == obj->interleavedRtcpIdM)
              obj->tunerM.ProcessRtcpData(data, count);
           }
        }
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

cString cSatipRtsp::GetActiveMode(void)
{
  switch (modeM) {
    case cSatipConfig::eTransportModeUnicast:
         return "Unicast";
    case cSatipConfig::eTransportModeMulticast:
         return "Multicast";
    case cSatipConfig::eTransportModeRtpOverTcp:
         return "RTP-over-TCP";
    default:
         break;
    }
  return "";
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

     // Limit download speed (bytes/s)
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_MAX_RECV_SPEED_LARGE, eMaxDownloadSpeedMBits * 131072L);

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

bool cSatipRtsp::SetInterface(const char *bindAddrP)
{
  debug1("%s (%s) [device %d]", __PRETTY_FUNCTION__, bindAddrP, tunerM.GetId());
  bool result = true;
  CURLcode res = CURLE_OK;

  if (handleM && !isempty(bindAddrP)) {
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERFACE, *cString::sprintf("host!%s", bindAddrP));
     }
  else {
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERFACE, NULL);
     }

  return result;
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

bool cSatipRtsp::Setup(const char *uriP, int rtpPortP, int rtcpPortP, bool useTcpP)
{
  debug1("%s (%s, %d, %d, %d) [device %d]", __PRETTY_FUNCTION__, uriP, rtpPortP, rtcpPortP, useTcpP, tunerM.GetId());
  bool result = false;

  if (handleM && !isempty(uriP)) {
     cString transport;
     long rc = 0;
     cTimeMs processing(0);
     CURLcode res = CURLE_OK;

     switch (SatipConfig.GetTransportMode()) {
       case cSatipConfig::eTransportModeMulticast:
            // RTP/AVP;multicast;destination=<multicast group address>;port=<RTP port>-<RTCP port>;ttl=<ttl>[;source=<multicast source address>]
            transport = cString::sprintf("RTP/AVP;multicast");
            break;
       default:
            // RTP/AVP;unicast;client_port=<client RTP port>-<client RTCP port>
            // RTP/AVP/TCP;unicast;client_port=<client RTP port>-<client RTCP port>
            if (useTcpP)
               transport = cString::sprintf("RTP/AVP/TCP;unicast;interleaved=%u-%u", interleavedRtpIdM, interleavedRtcpIdM);
            else
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
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipRtsp::DataCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERLEAVEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERLEAVEDATA, NULL);

     SATIP_CURL_EASY_PERFORM(handleM);
     // Session id is now known - disable header parsing
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_HEADERFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEHEADER, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);
     if (headerBufferM.Size() > 0) {
        ParseHeader();
        headerBufferM.Reset();
        }
     if (dataBufferM.Size() > 0) {
        ParseData();
        dataBufferM.Reset();
        }

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
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipRtsp::DataCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);
     if (dataBufferM.Size() > 0) {
        tunerM.ProcessApplicationData((u_char *)dataBufferM.Data(), dataBufferM.Size());
        dataBufferM.Reset();
        }

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
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipRtsp::DataCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_PERFORM(handleM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);
     if (dataBufferM.Size() > 0) {
        ParseData();
        dataBufferM.Reset();
        }

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
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipRtsp::DataCallback);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERLEAVEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERLEAVEDATA, NULL);
     SATIP_CURL_EASY_PERFORM(handleM);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, NULL);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, NULL);
     if (dataBufferM.Size() > 0) {
        ParseData();
        dataBufferM.Reset();
        }

     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_CLIENT_CSEQ, 1L);
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_RTSP_SESSION_ID, NULL);

     result = ValidateLatestResponse(&rc);
     debug5("%s (%s) Response %ld in %" PRIu64 " ms [device %d]", __PRETTY_FUNCTION__, uriP, rc, processing.Elapsed(), tunerM.GetId());
     }

  return result;
}

void cSatipRtsp::ParseHeader(void)
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  char *s, *p = headerBufferM.Data();
  char *r = strtok_r(p, "\r\n", &s);

  while (r) {
        debug16("%s (%zu): %s", __PRETTY_FUNCTION__, headerBufferM.Size(), r);
        r = skipspace(r);
        if (strstr(r, "com.ses.streamID")) {
           int streamid = -1;
           if (sscanf(r, "com.ses.streamID:%11d", &streamid) == 1)
              tunerM.SetStreamId(streamid);
           }
        else if (strstr(r, "Session:")) {
           int timeout = -1;
           char *session = NULL;
           if (sscanf(r, "Session:%m[^;];timeout=%11d", &session, &timeout) == 2)
              tunerM.SetSessionTimeout(skipspace(session), timeout * 1000);
           else if (sscanf(r, "Session:%m[^;]", &session) == 1)
              tunerM.SetSessionTimeout(skipspace(session), -1);
           FREE_POINTER(session);
           }
        else if (strstr(r, "Transport:")) {
           CURLcode res = CURLE_OK;
           int rtp = -1, rtcp = -1, ttl = -1;
           char *tmp = NULL, *destination = NULL, *source = NULL;
           interleavedRtpIdM = 0;
           interleavedRtcpIdM = 1;
           SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERLEAVEFUNCTION, NULL);
           SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERLEAVEDATA, NULL);
           if (sscanf(r, "Transport:%m[^;];unicast;client_port=%11d-%11d", &tmp, &rtp, &rtcp) == 3) {
              modeM = cSatipConfig::eTransportModeUnicast;
              tunerM.SetupTransport(rtp, rtcp, NULL, NULL);
              }
           else if (sscanf(r, "Transport:%m[^;];multicast;destination=%m[^;];port=%11d-%11d;ttl=%11d;source=%m[^;]", &tmp, &destination, &rtp, &rtcp, &ttl, &source) == 6 ||
                    sscanf(r, "Transport:%m[^;];multicast;destination=%m[^;];port=%11d-%11d;ttl=%11d", &tmp, &destination, &rtp, &rtcp, &ttl) == 5) {
              modeM = cSatipConfig::eTransportModeMulticast;
              tunerM.SetupTransport(rtp, rtcp, destination, source);
              }
           else if (sscanf(r, "Transport:%m[^;];interleaved=%11d-%11d", &tmp, &rtp, &rtcp) == 3) {
              interleavedRtpIdM = rtp;
              interleavedRtcpIdM = rtcp;
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERLEAVEFUNCTION, cSatipRtsp::InterleaveCallback);
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_INTERLEAVEDATA, this);
              modeM = cSatipConfig::eTransportModeRtpOverTcp;
              tunerM.SetupTransport(-1, -1, NULL, NULL);
              }
           FREE_POINTER(tmp);
           FREE_POINTER(destination);
           FREE_POINTER(source);
           }
        r = strtok_r(NULL, "\r\n", &s);
        }
}

void cSatipRtsp::ParseData(void)
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  char *s, *p = dataBufferM.Data();
  char *r = strtok_r(p, "\r\n", &s);

  while (r) {
        debug16("%s (%zu): %s", __PRETTY_FUNCTION__, dataBufferM.Size(), r);
        r = skipspace(r);
        if (strstr(r, "No-More:")) {
           char *tmp = NULL;
           if (sscanf(r, "No-More:%m[^;]", &tmp) == 1) {
              errorNoMoreM = skipspace(tmp);
              debug3("%s No-More: %s [device %d]", __PRETTY_FUNCTION__, *errorNoMoreM, tunerM.GetId());
              }
           FREE_POINTER(tmp);
           }
        else if (strstr(r, "Out-of-Range:")) {
           char *tmp = NULL;
           if (sscanf(r, "Out-of-Range:%m[^;]", &tmp) == 1) {
              errorOutOfRangeM = skipspace(tmp);
              debug3("%s Out-of-Range: %s [device %d]", __PRETTY_FUNCTION__, *errorOutOfRangeM, tunerM.GetId());
              }
           FREE_POINTER(tmp);
           }
        else if (strstr(r, "Check-Syntax:")) {
           char *tmp = NULL;
           if (sscanf(r, "Check-Syntax:%m[^;]", &tmp) == 1) {
              errorCheckSyntaxM = skipspace(tmp);
              debug3("%s Check-Syntax: %s [device %d]", __PRETTY_FUNCTION__, *errorCheckSyntaxM, tunerM.GetId());
              }
           FREE_POINTER(tmp);
           }
        r = strtok_r(NULL, "\r\n", &s);
        }
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
            if (!isempty(*errorCheckSyntaxM)) {
               SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_EFFECTIVE_URL, &url);
               error("Check syntax: %s (error code %ld: %s) [device %d]", *errorCheckSyntaxM, rc, url, tunerM.GetId());
               break;
               }
       case 403:
            // SETUP PLAY TEARDOWN
            // The message body of the response may contain the "Out-of-Range:" parameter followed
            // by a space-separated list of the attribute names that are not understood:
            // "src" "fe" "freq" "pol" "msys" "mtype" "plts" "ro" "sr" "fec" "pids" "addpids" "delpids" "mcast"
            if (!isempty(*errorOutOfRangeM)) {
               SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_EFFECTIVE_URL, &url);
               error("Out of range: %s (error code %ld: %s) [device %d]", *errorOutOfRangeM, rc, url, tunerM.GetId());
               // Reseting the connection wouldn't help anything due to invalid channel configuration, so let it be successful
               result = true;
               break;
               }
       case 503:
            // SETUP PLAY
            // The message body of the response may contain the "No-More:" parameter followed
            // by a space-separated list of the missing ressources: “sessions” "frontends" "pids
            if (!isempty(*errorNoMoreM)) {
               SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_EFFECTIVE_URL, &url);
               error("No more: %s (error code %ld: %s) [device %d]", *errorNoMoreM, rc, url, tunerM.GetId());
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
  errorNoMoreM = "";
  errorOutOfRangeM = "";
  errorCheckSyntaxM = "";
  debug1("%s result=%s [device %d]", __PRETTY_FUNCTION__, result ? "ok" : "failed", tunerM.GetId());

  return result;
}
