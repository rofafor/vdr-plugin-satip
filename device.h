/*
 * device.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_DEVICE_H
#define __SATIP_DEVICE_H

#include <vdr/device.h>
#include "common.h"
#include "deviceif.h"
#include "tuner.h"
#include "sectionfilter.h"
#include "statistics.h"

class cSatipDevice : public cDevice, public cSatipPidStatistics, public cSatipBufferStatistics, public cSatipDeviceIf {
  // static ones
public:
  static cMutex mutexS;
  static bool Initialize(unsigned int DeviceCount);
  static void Shutdown(void);
  static unsigned int Count(void);
  static cSatipDevice *GetSatipDevice(int CardIndex);
  static cString GetSatipStatus(void);

  // private parts
private:
  enum {
    eReadyTimeoutMs  = 2000, // in milliseconds
    eTuningTimeoutMs = 1000  // in milliseconds
  };
  unsigned int deviceIndexM;
  bool isPacketDeliveredM;
  bool isOpenDvrM;
  cString deviceNameM;
  cChannel channelM;
  cRingBufferLinear *tsBufferM;
  cSatipTuner *pTunerM;
  cSatipSectionFilterHandler *pSectionFilterHandlerM;
  cTimeMs createdM;
  cCondVar tunedM;

  // constructor & destructor
public:
  explicit cSatipDevice(unsigned int deviceIndexP);
  virtual ~cSatipDevice();
  cString GetInformation(unsigned int pageP = SATIP_DEVICE_INFO_ALL);

  // copy and assignment constructors
private:
  cSatipDevice(const cSatipDevice&);
  cSatipDevice& operator=(const cSatipDevice&);

  // for statistics and general information
  cString GetGeneralInformation(void);
  cString GetPidsInformation(void);
  cString GetFiltersInformation(void);

  // for channel info
public:
  virtual bool Ready(void);
  virtual cString DeviceType(void) const;
  virtual cString DeviceName(void) const;
  virtual bool AvoidRecording(void) const;
  virtual int SignalStrength(void) const;
  virtual int SignalQuality(void) const;

  // for channel selection
public:
  virtual bool ProvidesSource(int sourceP) const;
  virtual bool ProvidesTransponder(const cChannel *channelP) const;
  virtual bool ProvidesChannel(const cChannel *channelP, int priorityP = -1, bool *needsDetachReceiversP = NULL) const;
  virtual bool ProvidesEIT(void) const;
  virtual int NumProvidedSystems(void) const;
  virtual const cChannel *GetCurrentlyTunedTransponder(void) const;
  virtual bool IsTunedToTransponder(const cChannel *channelP) const;
  virtual bool MaySwitchTransponder(const cChannel *channelP) const;

protected:
  virtual bool SetChannelDevice(const cChannel *channelP, bool liveViewP);

  // for recording
private:
  uchar *GetData(int *availableP = NULL);
  void SkipData(int countP);

protected:
  virtual bool SetPid(cPidHandle *handleP, int typeP, bool onP);
  virtual bool OpenDvr(void);
  virtual void CloseDvr(void);
  virtual bool GetTSPacket(uchar *&dataP);

  // for section filtering
public:
  virtual int OpenFilter(u_short pidP, u_char tidP, u_char maskP);
  virtual void CloseFilter(int handleP);

  // for transponder lock
public:
  virtual bool HasLock(int timeoutMsP) const;

  // for common interface
public:
  virtual bool HasInternalCam(void);

  // for internal device interface
public:
  virtual void WriteData(u_char *bufferP, int lengthP);
  virtual void SetChannelTuned(void);
  virtual int GetId(void);
  virtual int GetPmtPid(void);
  virtual int GetCISlot(void);
  virtual cString GetTnrParameterString(void);
  virtual bool IsIdle(void);
};

#endif // __SATIP_DEVICE_H
