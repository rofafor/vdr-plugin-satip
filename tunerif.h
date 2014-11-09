/*
 * tunerif.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_TUNERIF_H
#define __SATIP_TUNERIF_H

class cSatipTunerIf {
public:
  cSatipTunerIf() {}
  virtual ~cSatipTunerIf() {}
  virtual void ParseReceptionParameters(const char *paramP) = 0;
  virtual void SetStreamId(int streamIdP) = 0;
  virtual void SetSessionTimeout(const char *sessionP, int timeoutP) = 0;
  virtual int GetId(void) = 0;

private:
  cSatipTunerIf(const cSatipTunerIf&);
  cSatipTunerIf& operator=(const cSatipTunerIf&);
};

#endif // __SATIP_TUNERIF_H
