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
  tunersM(new cSatipPollerTuners()),
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
  DELETENULL(tunersM);
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
        if (nfds == -1) {
           error("epoll_wait() failed");
           }
        else if (nfds > 0) {
           for (int i = 0; i < nfds; ++i) {
               for (cSatipPollerTuner *tuner = tunersM->First(); tuner; tuner = tunersM->Next(tuner)) {
                   if (events[i].data.fd == tuner->VideoFd()) {
                      tuner->Poller()->ReadVideo();
                      break;
                      }
                   else if (events[i].data.fd == tuner->ApplicationFd()) {
                      tuner->Poller()->ReadApplication();
                      break;
                      }
                   }
               }
           }
        }
  debug("cSatipPoller::%s(): exiting", __FUNCTION__);
}

bool cSatipPoller::Register(cSatipPollerIf &pollerP)
{
  debug("cSatipPoller::%s(%d)", __FUNCTION__, pollerP.GetPollerId());
  cMutexLock MutexLock(&mutexM);
  if (tunersM && (fdM >= 0)) {
     bool found = false;
     for (cSatipPollerTuner *tuner = tunersM->First(); tuner; tuner = tunersM->Next(tuner)) {
         if (tuner->Poller() == &pollerP) {
            found = true;
            break;
            }
         }
     if (!found) {
        cSatipPollerTuner *tmp = new cSatipPollerTuner(pollerP, pollerP.GetVideoFd(), pollerP.GetApplicationFd());
        if (tmp) {
           tunersM->Add(tmp);
           if (tmp->VideoFd() >= 0) {
              struct epoll_event ev;
              ev.events = EPOLLIN | EPOLLET;
              ev.data.fd = tmp->VideoFd();
              if (epoll_ctl(fdM, EPOLL_CTL_ADD, pollerP.GetVideoFd(), &ev) == -1) {
                 error("Cannot add video socket into epoll [device %d]", pollerP.GetPollerId());
                 }
              }
           if (tmp->ApplicationFd() >= 0) {
              struct epoll_event ev;
              ev.events = EPOLLIN | EPOLLET;
              ev.data.fd = tmp->ApplicationFd();
              if (epoll_ctl(fdM, EPOLL_CTL_ADD, tmp->ApplicationFd(), &ev) == -1) {
                 error("Cannot add application socket into epoll [device %d]", pollerP.GetPollerId());
                 }
              }
           debug("cSatipPoller::%s(%d): Added interface", __FUNCTION__, pollerP.GetPollerId());
           }
        }
     return true;
     }
  return false;
}

bool cSatipPoller::Unregister(cSatipPollerIf &pollerP)
{
  debug("cSatipPoller::%s(%d)", __FUNCTION__, pollerP.GetPollerId());
  cMutexLock MutexLock(&mutexM);
  if (tunersM && (fdM >= 0)) {
     for (cSatipPollerTuner *tuner = tunersM->First(); tuner; tuner = tunersM->Next(tuner)) {
         if (tuner->Poller() == &pollerP) {
            if ((tuner->VideoFd() >= 0) && (epoll_ctl(fdM, EPOLL_CTL_DEL, tuner->VideoFd(), NULL) == -1)) {
               error("Cannot remove video socket from epoll [device %d]", pollerP.GetPollerId());
               }
            if ((tuner->ApplicationFd() >= 0) && (epoll_ctl(fdM, EPOLL_CTL_DEL, tuner->ApplicationFd(), NULL) == -1)) {
               error("Cannot remove application socket from epoll [device %d]", pollerP.GetPollerId());
               }
            tunersM->Del(tuner);
            debug("cSatipPoller::%s(%d): Removed interface", __FUNCTION__, pollerP.GetPollerId());
            return true;
            }
         }
     }
  return false;
}

