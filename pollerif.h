/*
 * pollerif.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_POLLERIF_H
#define __SATIP_POLLERIF_H

class cSatipPollerIf {
public:
  cSatipPollerIf() {}
  virtual ~cSatipPollerIf() {}
  virtual int GetFd(void) = 0;
  virtual void Process(void) = 0;

private:
  cSatipPollerIf(const cSatipPollerIf&);
  cSatipPollerIf& operator=(const cSatipPollerIf&);
};

#endif // __SATIP_POLLERIF_H
