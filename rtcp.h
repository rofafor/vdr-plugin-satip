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
  enum {
    eApplicationMaxSizeB = 1500,
  };
  cSatipTunerIf &tunerM;
  unsigned int bufferLenM;
  unsigned char *bufferM;
  int GetApplicationOffset(int *lengthP);

public:
  explicit cSatipRtcp(cSatipTunerIf &tunerP);
  virtual ~cSatipRtcp();

  // for internal poller interface
public:
  virtual int GetFd(void);
  virtual void Process(void);
  virtual cString ToString(void) const;
};

#endif /* __SATIP_RTCP_H_ */
