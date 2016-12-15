/*
 * msearch.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_MSEARCH_H_
#define __SATIP_MSEARCH_H_

#include "discoverif.h"
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
  cSatipDiscoverIf &discoverM;
  unsigned int bufferLenM;
  unsigned char *bufferM;
  bool registeredM;

public:
  explicit cSatipMsearch(cSatipDiscoverIf &discoverP);
  virtual ~cSatipMsearch();
  void Probe(void);

  // for internal poller interface
public:
  virtual int GetFd(void);
  virtual void Process(void);
  virtual void Process(unsigned char *dataP, int lengthP);
  virtual cString ToString(void) const;
};

#endif /* __SATIP_MSEARCH_H_ */
