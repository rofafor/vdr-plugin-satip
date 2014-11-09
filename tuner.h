/*
 * tuner.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_TUNER_H
#define __SATIP_TUNER_H

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "deviceif.h"
#include "server.h"
#include "statistics.h"
#include "socket.h"
#include "data.h"

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
  static void   DataTimeoutCallback(void *objP);
  static int    DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP);

  cSatipTunerDataThread dataThreadM;
  cCondWait sleepM;
  cSatipDeviceIf* deviceM;
  cSatipSocket *rtpSocketM;
  cSatipSocket *rtcpSocketM;
  cString streamAddrM;
  cString streamParamM;
  cSatipServer *currentServerM;
  cSatipServer *nextServerM;
  cMutex mutexM;
  CURL *handleM;
  cTimeMs keepAliveM;
  cTimeMs statusUpdateM;
  cTimeMs signalInfoCacheM;
  cTimeMs pidUpdateCacheM;
  cString sessionM;
  int timeoutM;
  bool reconnectM;
  bool tunedM;
  bool hasLockM;
  int signalStrengthM;
  int signalQualityM;
  int streamIdM;
  cSatipVector<int> addPidsM;
  cSatipVector<int> delPidsM;
  cSatipVector<int> pidsM;

  bool Connect(void);
  bool Disconnect(void);
  bool ValidateLatestResponse(void);
  void ParseReceptionParameters(const char *paramP);
  void SetStreamId(int streamIdP);
  void SetSessionTimeout(const char *sessionP, int timeoutP = 0);
  bool KeepAlive(void);
  bool UpdateSignalInfoCache(void);
  bool UpdatePids(bool forceP = false);
  cString GeneratePidParameter(bool allPidsP = false);

  bool RtspInitialize(void);
  bool RtspTerminate(void);
  bool RtspOptions(void);
  bool RtspSetup(const char *paramP, int rtpPortP, int rtcpPortP);
  bool RtspDescribe(void);
  bool RtspPlay(const char *paramP = NULL);
  bool RtspTeardown(void);

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
