/*
 * rtp.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_RTP_H_
#define __SATIP_RTP_H_

#include "socket.h"
#include "deviceif.h"
#include "pollerif.h"
#include "statistics.h"

class cSatipRtp : public cSatipSocket, public cSatipPollerIf {
private:
  enum {
    eReportIntervalS = 300 // in seconds
  };
  cSatipDeviceIf *deviceM;
  unsigned int bufferLenM;
  unsigned char *bufferM;
  time_t lastErrorReportM;
  int packetErrorsM;
  int sequenceNumberM;
  int GetHeaderLenght(int lengthP);

protected:
  virtual int GetFd(void);
  virtual void Action(int fdP);

public:
  cSatipRtp(cSatipDeviceIf &deviceP, unsigned int bufferLenP);
  virtual ~cSatipRtp();
  virtual void Close(void);
};

#endif /* __SATIP_RTP_H_ */
