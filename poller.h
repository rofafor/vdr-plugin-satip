/*
 * poller.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_POLLER_H
#define __SATIP_POLLER_H

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "pollerif.h"

class cSatipPoller : public cThread {
private:
  enum {
    eMaxFileDescriptors = SATIP_MAX_DEVICES * 2, // Data + Application
  };
  static cSatipPoller *instanceS;
  cMutex mutexM;
  int fdM;
  void Activate(void);
  void Deactivate(void);
  // constructor
  cSatipPoller();
  // to prevent copy constructor and assignment
  cSatipPoller(const cSatipPoller&);
  cSatipPoller& operator=(const cSatipPoller&);

protected:
  virtual void Action(void);

public:
  static cSatipPoller *GetInstance(void);
  static bool Initialize(void);
  static void Destroy(void);
  virtual ~cSatipPoller();
  bool Register(cSatipPollerIf &pollerP);
  bool Unregister(cSatipPollerIf &pollerP);
};

#endif // __SATIP_POLLER_H
