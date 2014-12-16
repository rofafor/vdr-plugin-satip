/*
 * rtsp.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_RTSP_H
#define __SATIP_RTSP_H

#include <curl/curl.h>
#include <curl/easy.h>

#ifndef CURLOPT_RTSPHEADER
#error "libcurl is missing required RTSP support"
#endif

#include "tunerif.h"

class cSatipRtsp {
private:
  static size_t HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  static size_t WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  static int    DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP);

  enum {
    eConnectTimeoutMs = 1500,  // in milliseconds
  };

  cSatipTunerIf &tunerM;
  CURL *handleM;
  struct curl_slist *headerListM;

  void Create(void);
  void Destroy(void);
  bool ValidateLatestResponse(long *rcP);

  // to prevent copy constructor and assignment
  cSatipRtsp(const cSatipRtsp&);
  cSatipRtsp& operator=(const cSatipRtsp&);

public:
  cSatipRtsp(cSatipTunerIf &tunerP);
  virtual ~cSatipRtsp();

  cString RtspUnescapeString(const char *strP);
  void Reset(void);
  bool Options(const char *uriP);
  bool Setup(const char *uriP, int rtpPortP, int rtcpPortP);
  bool SetSession(const char *sessionP);
  bool Describe(const char *uriP);
  bool Play(const char *uriP);
  bool Teardown(const char *uriP);
};

#endif // __SATIP_RTSP_H
