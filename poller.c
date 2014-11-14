/*
 * poller.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <sys/epoll.h>

#include "common.h"
#include "poller.h"

cSatipPoller *cSatipPoller::instanceS = NULL;

cSatipPoller *cSatipPoller::GetInstance(void)
{
  if (!instanceS)
     instanceS = new cSatipPoller();
  return instanceS;
}

bool cSatipPoller::Initialize(void)
{
  debug("cSatipPoller::%s()", __FUNCTION__);
  if (instanceS)
     instanceS->Activate();
  return true;
}

void cSatipPoller::Destroy(void)
{
  debug("cSatipPoller::%s()", __FUNCTION__);
  if (instanceS)
     instanceS->Deactivate();
}

cSatipPoller::cSatipPoller()
: cThread("SAT>IP poller"),
  mutexM(),
  fdM(epoll_create(eMaxFileDescriptors))
{
  debug("cSatipPoller::%s()", __FUNCTION__);
}

cSatipPoller::~cSatipPoller()
{
  debug("cSatipPoller::%s()", __FUNCTION__);
  Deactivate();
  cMutexLock MutexLock(&mutexM);
  close(fdM);
  // Free allocated memory
}

void cSatipPoller::Activate(void)
{
  // Start the thread
  Start();
}

void cSatipPoller::Deactivate(void)
{
  debug("cSatipPoller::%s()", __FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  if (Running())
     Cancel(3);
}

void cSatipPoller::Action(void)
{
  debug("cSatipPoller::%s(): entering", __FUNCTION__);
  struct epoll_event events[eMaxFileDescriptors];
  // Increase priority
  SetPriority(-1);
  // Do the thread loop
  while (Running()) {
        int nfds = epoll_wait(fdM, events, eMaxFileDescriptors, -1);
        ERROR_IF_FUNC((nfds == -1), "epoll_wait() failed", break, ;);
        for (int i = 0; i < nfds; ++i) {
            cSatipPollerIf* poll = reinterpret_cast<cSatipPollerIf *>(events[i].data.ptr);
            if (poll)
               poll->Action(events[i].events);
           }
        }
  debug("cSatipPoller::%s(): exiting", __FUNCTION__);
}

bool cSatipPoller::Register(cSatipPollerIf &pollerP)
{
  debug("cSatipPoller::%s(%d)", __FUNCTION__, pollerP.GetFd());
  cMutexLock MutexLock(&mutexM);

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.ptr = &pollerP;
  ERROR_IF_RET(epoll_ctl(fdM, EPOLL_CTL_ADD, pollerP.GetFd(), &ev) == -1, "epoll_ctl(EPOLL_CTL_ADD) failed", return false);
  debug("cSatipPoller::%s(%d): Added interface", __FUNCTION__, pollerP.GetFd());

  return true;
}

bool cSatipPoller::Unregister(cSatipPollerIf &pollerP)
{
  debug("cSatipPoller::%s(%d)", __FUNCTION__, pollerP.GetFd());
  cMutexLock MutexLock(&mutexM);
  ERROR_IF_RET((epoll_ctl(fdM, EPOLL_CTL_DEL, pollerP.GetFd(), NULL) == -1), "epoll_ctl(EPOLL_CTL_DEL) failed", return false);
  debug("cSatipPoller::%s(%d): Removed interface", __FUNCTION__, pollerP.GetFd());

  return true;
}
