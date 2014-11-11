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
  virtual void ReadVideo(void) = 0;
  virtual void ReadApplication(void) = 0;
  virtual int GetPollerId(void) = 0;
  virtual int GetVideoFd(void) = 0;
  virtual int GetApplicationFd(void) = 0;

private:
  cSatipPollerIf(const cSatipPollerIf&);
  cSatipPollerIf& operator=(const cSatipPollerIf&);
};

#endif // __SATIP_POLLERIF_H
