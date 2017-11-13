/*
 * deviceif.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_DEVICEIF_H
#define __SATIP_DEVICEIF_H

class cSatipDeviceIf {
public:
  cSatipDeviceIf() {}
  virtual ~cSatipDeviceIf() {}
  virtual void WriteData(u_char *bufferP, int lengthP) = 0;
  virtual void SetChannelTuned(void) = 0;
  virtual int GetId(void) = 0;
  virtual int GetPmtPid(void) = 0;
  virtual int GetCISlot(void) = 0;
  virtual cString GetTnrParameterString(void) = 0;
  virtual bool IsIdle(void) = 0;

private:
  explicit cSatipDeviceIf(const cSatipDeviceIf&);
  cSatipDeviceIf& operator=(const cSatipDeviceIf&);
};

#endif // __SATIP_DEVICEIF_H
