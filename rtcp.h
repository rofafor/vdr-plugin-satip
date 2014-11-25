/*
 * rtcp.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_RTCP_H_
#define __SATIP_RTCP_H_

#include "socket.h"
#include "tunerif.h"
#include "pollerif.h"

class cSatipRtcp : public cSatipSocket, public cSatipPollerIf {
private:
  cSatipTunerIf &tunerM;
  unsigned int bufferLenM;
  unsigned char *bufferM;
  int GetApplicationOffset(int *lenghtP);

public:
  cSatipRtcp(cSatipTunerIf &tunerP, unsigned int bufferLenP);
  virtual ~cSatipRtcp();

  // for internal poller interface
public:
  virtual int GetFd(void);
  virtual void Process(void);
};

#endif /* __SATIP_RTCP_H_ */
