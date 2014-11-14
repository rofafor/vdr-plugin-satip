/*
 * rtcp.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_RTCP_H_
#define __SATIP_RTCP_H_

#include "common.h"
#include "socket.h"
#include "tunerif.h"
#include "pollerif.h"

class cSatipRtcp : public cSatipSocket, public cSatipPollerIf
{
private:
  cSatipTunerIf *tunerM;
  unsigned int bufferLenM;
  unsigned char *bufferM;

protected:
  virtual int GetFd(void);
  virtual void Action(int fdP);

public:
  cSatipRtcp(cSatipTunerIf &tunerP, unsigned int bufferLenP);
  virtual ~cSatipRtcp();
};

#endif /* __SATIP_RTCP_H_ */
