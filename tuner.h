/*
 * tuner.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_TUNER_H
#define __SATIP_TUNER_H

#include <vdr/config.h> // APIVERSNUM
#include <vdr/thread.h>
#include <vdr/tools.h>

#include "deviceif.h"
#include "discover.h"
#include "rtp.h"
#include "rtcp.h"
#include "rtsp.h"
#include "server.h"
#include "statistics.h"

class cSatipPid : public cVector<int> {
private:
  static int PidCompare(const void *aPidP, const void *bPidP)
  {
    return (*(int*)aPidP - *(int*)bPidP);
  }

public:
#if defined(APIVERSNUM) && APIVERSNUM < 20107
  int IndexOf(const int &pidP)
  {
    for (int i = 0; i < Size(); ++i) {
        if (pidP == At(i))
           return i;
        }
    return -1;
  }
  bool RemoveElement(const int &pidP)
  {
    int i = IndexOf(pidP);
    if (i >= 0) {
       Remove(i);
       return true;
       }
    return false;
  }
  bool AppendUnique(int pidP)
  {
    if (IndexOf(pidP) < 0) {
       Append(pidP);
       return true;
       }
    return false;
  }
#endif
  void RemovePid(const int &pidP)
  {
    if (RemoveElement(pidP))
       Sort(PidCompare);
  }
  void AddPid(int pidP)
  {
    if (AppendUnique(pidP))
       Sort(PidCompare);
  }
  cString ListPids(void)
  {
    cString list = "";
    if (Size()) {
       for (int i = 0; i < Size(); ++i)
           list = cString::sprintf("%s%d,", *list, At(i));
       list = list.Truncate(-1);
       }
    return list;
  }
};

class cSatipTunerServer
{
private:
  cSatipServer *serverM;
  int transponderM;

public:
  cSatipTunerServer(cSatipServer *serverP, const int transponderP) : serverM(serverP), transponderM(transponderP) {}
  ~cSatipTunerServer() {}
  cSatipTunerServer(const cSatipTunerServer &objP) { serverM = NULL; transponderM = 0; }
  cSatipTunerServer& operator= (const cSatipTunerServer &objP) { serverM = objP.serverM; transponderM = objP.transponderM; return *this; }
  bool IsValid(void) { return !!serverM; }
  bool IsQuirk(int quirkP) { return (serverM && cSatipDiscover::GetInstance()->IsServerQuirk(serverM, quirkP)); }
  void Use(bool onOffP) { if (serverM) cSatipDiscover::GetInstance()->UseServer(serverM, transponderM, onOffP); }
  void Set(cSatipServer *serverP, const int transponderP) { serverM = serverP; transponderM = transponderP; }
  void Reset(void) { serverM = NULL; transponderM = 0; }
  cString GetAddress(void) { return serverM ? cSatipDiscover::GetInstance()->GetServerAddress(serverM) : ""; }
  cString GetInfo(void) { return cString::sprintf("server=%s transponder=%d", serverM ? "assigned" : "null", transponderM); }
};

class cSatipTuner : public cThread, public cSatipTunerStatistics, public cSatipTunerIf
{
private:
  enum {
    eDummyPid               = 100,
    eDefaultSignalStrength  = 15,
    eDefaultSignalQuality   = 224,
    eSleepTimeoutMs         = 250,   // in milliseconds
    eStatusUpdateTimeoutMs  = 1000,  // in milliseconds
    ePidUpdateIntervalMs    = 250,   // in milliseconds
    eConnectTimeoutMs       = 5000,  // in milliseconds
    eMinKeepAliveIntervalMs = 30000  // in milliseconds
  };
  enum eTunerState { tsIdle, tsRelease, tsSet, tsTuned, tsLocked };
  enum eStateMode { smInternal, smExternal };

  cCondWait sleepM;
  cSatipDeviceIf* deviceM;
  int deviceIdM;
  cSatipRtsp rtspM;
  cSatipRtp rtpM;
  cSatipRtcp rtcpM;
  cString streamAddrM;
  cString streamParamM;
  cSatipTunerServer currentServerM;
  cSatipTunerServer nextServerM;
  cMutex mutexM;
  cTimeMs reConnectM;
  cTimeMs keepAliveM;
  cTimeMs statusUpdateM;
  cTimeMs pidUpdateCacheM;
  cString sessionM;
  eTunerState currentStateM;
  cVector<eTunerState> internalStateM;
  cVector<eTunerState> externalStateM;
  int timeoutM;
  bool hasLockM;
  int signalStrengthM;
  int signalQualityM;
  int frontendIdM;
  int streamIdM;
  int pmtPidM;
  cSatipPid addPidsM;
  cSatipPid delPidsM;
  cSatipPid pidsM;

  bool Connect(void);
  bool Disconnect(void);
  bool KeepAlive(bool forceP = false);
  bool ReadReceptionStatus(bool forceP = false);
  bool UpdatePids(bool forceP = false);
  void UpdateCurrentState(void);
  bool StateRequested(void);
  bool RequestState(eTunerState stateP, eStateMode modeP);
  const char *StateModeString(eStateMode modeP);
  const char *TunerStateString(eTunerState stateP);

protected:
  virtual void Action(void);

public:
  cSatipTuner(cSatipDeviceIf &deviceP, unsigned int packetLenP);
  virtual ~cSatipTuner();
  bool IsTuned(void) const { return (currentStateM >= tsTuned); }
  bool SetSource(cSatipServer *serverP, const int transponderP, const char *parameterP, const int indexP);
  bool SetPid(int pidP, int typeP, bool onP);
  bool Open(void);
  bool Close(void);
  int FrontendId(void);
  int SignalStrength(void);
  int SignalQuality(void);
  bool HasLock(void);
  cString GetSignalStatus(void);
  cString GetInformation(void);

  // for internal tuner interface
public:
  virtual void ProcessVideoData(u_char *bufferP, int lengthP);
  virtual void ProcessApplicationData(u_char *bufferP, int lengthP);
  virtual void SetStreamId(int streamIdP);
  virtual void SetSessionTimeout(const char *sessionP, int timeoutP);
  virtual int GetId(void);
};

#endif // __SATIP_TUNER_H
