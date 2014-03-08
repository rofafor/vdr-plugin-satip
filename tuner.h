/*
 * tuner.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_TUNER_H
#define __SATIP_TUNER_H

#include <curl/curl.h>
#include <curl/easy.h>

#ifndef CURLOPT_RTSPHEADER
#error "libcurl is missing required RTSP support"
#endif

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "deviceif.h"
#include "statistics.h"
#include "socket.h"

class cSatipTuner : public cThread, public cSatipTunerStatistics {
private:
  enum {
    eConnectTimeoutMs    = 1500,  // in milliseconds
    ePidUpdateIntervalMs = 100,   // in milliseconds
    eReConnectTimeoutMs  = 5000,  // in milliseconds
    eKeepAliveIntervalMs = 600000 // in milliseconds
  };

  static size_t HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);

  cCondWait sleepM;
  cSatipDeviceIf* deviceM;
  unsigned char* packetBufferM;
  unsigned int packetBufferLenM;
  cSatipSocket *rtpSocketM;
  cSatipSocket *rtcpSocketM;
  cString streamAddrM;
  cString streamParamM;
  cMutex mutexM;
  CURL *handleM;
  struct curl_slist *headerListM;
  cTimeMs keepAliveM;
  cTimeMs signalInfoCacheM;
  cTimeMs pidUpdateCacheM;
  int timeoutM;
  bool openedM;
  bool tunedM;
  bool hasLockM;
  int signalStrengthM;
  int signalQualityM;
  int streamIdM;
  bool pidUpdatedM;
  cVector<int> pidsM;

  bool Connect(void);
  bool Disconnect(void);
  bool ValidateLatestResponse(void);
  void ParseReceptionParameters(const char *paramP);
  void SetStreamInfo(int idP, int timeoutP);
  bool KeepAlive(void);
  bool UpdateSignalInfoCache(void);
  bool UpdatePids(void);

protected:
  virtual void Action(void);

public:
  cSatipTuner(cSatipDeviceIf &deviceP, unsigned int packetLenP);
  virtual ~cSatipTuner();
  bool IsTuned(void) const { return tunedM; }
  bool SetSource(const char* addressP, const char *parameterP, const int indexP);
  bool SetPid(int pidP, int typeP, bool onP);
  bool Open(void);
  bool Close(void);
  int SignalStrength(void);
  int SignalQuality(void);
  bool HasLock(void);
  cString GetSignalStatus(void);
  cString GetInformation(void);
};

#endif // __SATIP_TUNER_H
