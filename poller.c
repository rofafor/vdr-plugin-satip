/*
 * poller.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>
#include <sys/epoll.h>

#include "config.h"
#include "common.h"
#include "log.h"
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
  debug1("%s", __PRETTY_FUNCTION__);
  if (instanceS)
     instanceS->Activate();
  return true;
}

void cSatipPoller::Destroy(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (instanceS)
     instanceS->Deactivate();
}

cSatipPoller::cSatipPoller()
: cThread("SATIP poller"),
  mutexM(),
  fdM(epoll_create(eMaxFileDescriptors))
{
  debug1("%s", __PRETTY_FUNCTION__);
}

cSatipPoller::~cSatipPoller()
{
  debug1("%s", __PRETTY_FUNCTION__);
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
  debug1("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  if (Running())
     Cancel(3);
}

void cSatipPoller::Action(void)
{
  debug1("%s Entering", __PRETTY_FUNCTION__);
  struct epoll_event events[eMaxFileDescriptors];
  uint64_t maxElapsed = 0;
  // Increase priority
  SetPriority(-1);
  // Do the thread loop
  while (Running()) {
        int nfds = epoll_wait(fdM, events, eMaxFileDescriptors, -1);
        ERROR_IF_FUNC((nfds == -1), "epoll_wait() failed", break, ;);
        for (int i = 0; i < nfds; ++i) {
            cSatipPollerIf* poll = reinterpret_cast<cSatipPollerIf *>(events[i].data.ptr);
            if (poll) {
               uint64_t elapsed;
               cTimeMs processing(0);
               poll->Process();
               elapsed = processing.Elapsed();
               if (elapsed > maxElapsed) {
                  maxElapsed = elapsed;
                  debug1("%s Processing %s took %" PRIu64 " ms", __PRETTY_FUNCTION__, *(poll->ToString()), maxElapsed);
                  }
               }
           }
        }
  debug1("%s Exiting", __PRETTY_FUNCTION__);
}

bool cSatipPoller::Register(cSatipPollerIf &pollerP)
{
  debug1("%s fd=%d", __PRETTY_FUNCTION__, pollerP.GetFd());
  cMutexLock MutexLock(&mutexM);

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.ptr = &pollerP;
  ERROR_IF_RET(epoll_ctl(fdM, EPOLL_CTL_ADD, pollerP.GetFd(), &ev) == -1, "epoll_ctl(EPOLL_CTL_ADD) failed", return false);
  debug1("%s Added interface fd=%d", __PRETTY_FUNCTION__, pollerP.GetFd());

  return true;
}

bool cSatipPoller::Unregister(cSatipPollerIf &pollerP)
{
  debug1("%s fd=%d", __PRETTY_FUNCTION__, pollerP.GetFd());
  cMutexLock MutexLock(&mutexM);
  ERROR_IF_RET((epoll_ctl(fdM, EPOLL_CTL_DEL, pollerP.GetFd(), NULL) == -1), "epoll_ctl(EPOLL_CTL_DEL) failed", return false);
  debug1("%s Removed interface fd=%d", __PRETTY_FUNCTION__, pollerP.GetFd());

  return true;
}
