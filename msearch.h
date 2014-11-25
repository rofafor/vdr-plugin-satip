/*
 * msearch.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_MSEARCH_H_
#define __SATIP_MSEARCH_H_

#include "socket.h"
#include "pollerif.h"

class cSatipMsearch : public cSatipSocket, public cSatipPollerIf {
private:
  enum {
    eProbeBufferSize  = 1024, // in bytes
    eDiscoveryPort    = 1900,
  };
  static const char *bcastAddressS;
  static const char *bcastMessageS;
  unsigned int bufferLenM;
  unsigned char *bufferM;
  bool registeredM;

public:
  cSatipMsearch(void);
  virtual ~cSatipMsearch();
  void Probe(void);

  // for internal poller interface
public:
  virtual int GetFd(void);
  virtual void Process(void);
};

#endif /* __SATIP_MSEARCH_H_ */
