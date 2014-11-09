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
#include "server.h"
#include "statistics.h"
#include "socket.h"

class cSatipPid : public cVector<int> {
private:
  int PidIndex(const int &pidP)
  {
    for (int i = 0; i < this->Size(); ++i) {
        if (pidP == this->At(i))
           return i;
        }
    return -1;
  }

public:
  void RemovePid(const int &pidP)
  {
    int i = PidIndex(pidP);
    if (i >= 0)
       this->Remove(i);
  }

  void AddPid(int pidP)
  {
    if (PidIndex(pidP) < 0)
       this->Append(pidP);
  }
};

class cSatipTuner : public cThread, public cSatipTunerStatistics {
private:
  enum {
    eDummyPid               = 100,
    eDefaultSignalStrength  = 15,
    eDefaultSignalQuality   = 224,
    eStatusUpdateTimeoutMs  = 1000,  // in milliseconds
    eConnectTimeoutMs       = 1500,  // in milliseconds
    ePidUpdateIntervalMs    = 250,   // in milliseconds
    eReConnectTimeoutMs     = 5000,  // in milliseconds
    eMinKeepAliveIntervalMs = 30000  // in milliseconds
  };

  static size_t HeaderCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  static size_t DataCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  static int    DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP);

  cCondWait sleepM;
  cSatipDeviceIf* deviceM;
  unsigned char* packetBufferM;
  unsigned int packetBufferLenM;
  cSatipSocket *rtpSocketM;
  cSatipSocket *rtcpSocketM;
  cString streamAddrM;
  cString streamParamM;
  cSatipServer *currentServerM;
  cSatipServer *nextServerM;
  cMutex mutexM;
  CURL *handleM;
  struct curl_slist *headerListM;
  cTimeMs keepAliveM;
  cTimeMs statusUpdateM;
  cTimeMs signalInfoCacheM;
  cTimeMs pidUpdateCacheM;
  cString sessionM;
  int timeoutM;
  bool openedM;
  bool tunedM;
  bool hasLockM;
  int signalStrengthM;
  int signalQualityM;
  int streamIdM;
  cSatipPid addPidsM;
  cSatipPid delPidsM;
  cSatipPid pidsM;

  bool Connect(void);
  bool Disconnect(void);
  bool ValidateLatestResponse(void);
  void ParseReceptionParameters(const char *paramP);
  void SetStreamId(int streamIdP);
  void SetSessionTimeout(const char *sessionP, int timeoutP = 0);
  bool KeepAlive(void);
  bool ReadReceptionStatus(void);
  bool UpdateSignalInfoCache(void);
  bool UpdatePids(bool forceP = false);

protected:
  virtual void Action(void);

public:
  cSatipTuner(cSatipDeviceIf &deviceP, unsigned int packetLenP);
  virtual ~cSatipTuner();
  bool IsTuned(void) const { return tunedM; }
  bool SetSource(cSatipServer *serverP, const char *parameterP, const int indexP);
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
