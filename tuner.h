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
#include "rtp.h"
#include "rtcp.h"
#include "rtsp.h"
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

class cSatipTuner : public cThread, public cSatipTunerStatistics, public cSatipTunerIf
{
private:
  enum {
    eDummyPid               = 100,
    eDefaultSignalStrength  = 15,
    eDefaultSignalQuality   = 224,
    eStatusUpdateTimeoutMs  = 1000,  // in milliseconds
    ePidUpdateIntervalMs    = 250,   // in milliseconds
    eConnectTimeoutMs       = 5000,  // in milliseconds
    eMinKeepAliveIntervalMs = 30000  // in milliseconds
  };
  enum eTunerStatus { tsIdle, tsRelease, tsSet, tsTuned, tsLocked };

  cCondWait sleepM;
  cSatipDeviceIf* deviceM;
  int deviceIdM;
  cSatipRtsp *rtspM;
  cSatipRtp *rtpM;
  cSatipRtcp *rtcpM;
  cString streamAddrM;
  cString streamParamM;
  cSatipServer *currentServerM;
  cSatipServer *nextServerM;
  cMutex mutexM;
  cTimeMs keepAliveM;
  cTimeMs statusUpdateM;
  cTimeMs pidUpdateCacheM;
  cString sessionM;
  eTunerStatus tunerStatusM;
  int timeoutM;
  bool hasLockM;
  int signalStrengthM;
  int signalQualityM;
  int streamIdM;
  cSatipPid addPidsM;
  cSatipPid delPidsM;
  cSatipPid pidsM;

  bool Connect(void);
  bool Disconnect(void);
  bool KeepAlive(bool forceP = false);
  bool ReadReceptionStatus(bool forceP = false);
  bool UpdatePids(bool forceP = false);
  const char *TunerStatusString(eTunerStatus statusP);

protected:
  virtual void Action(void);

public:
  cSatipTuner(cSatipDeviceIf &deviceP, unsigned int packetLenP);
  virtual ~cSatipTuner();
  bool IsTuned(void) const { return tunerStatusM > tsIdle; }
  bool SetSource(cSatipServer *serverP, const char *parameterP, const int indexP);
  bool SetPid(int pidP, int typeP, bool onP);
  bool Open(void);
  bool Close(void);
  int SignalStrength(void);
  int SignalQuality(void);
  bool HasLock(void);
  cString GetSignalStatus(void);
  cString GetInformation(void);

  // for internal tuner interface
public:
  virtual void ParseReceptionParameters(u_char *bufferP, int lengthP);
  virtual void SetStreamId(int streamIdP);
  virtual void SetSessionTimeout(const char *sessionP, int timeoutP);
  virtual int GetId(void);
};

#endif // __SATIP_TUNER_H
