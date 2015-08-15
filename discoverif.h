/*
 * discoverif.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_DISCOVERIF_H
#define __SATIP_DISCOVERIF_H

class cSatipDiscoverIf {
public:
  cSatipDiscoverIf() {}
  virtual ~cSatipDiscoverIf() {}
  virtual void SetUrl(const char *urlP) = 0;

private:
  explicit cSatipDiscoverIf(const cSatipDiscoverIf&);
  cSatipDiscoverIf& operator=(const cSatipDiscoverIf&);
};

#endif // __SATIP_DISCOVERIF_H
