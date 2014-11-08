/*
 * tuner.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "config.h"
#include "discover.h"
#include "param.h"
#include "tuner.h"
#include "data.h"

enum LOGLEVEL {
   logFunc = 0x01,
   logFuncPerf = 0x02,
   logData = 0x10,
   logDataDetails = 0x20,
   logAll = 0xFFFF
   };

int logLevel = logFunc | logFuncPerf | logData;

#define log(lvl) \
   if (logLevel & lvl) \
      debug("%s::%s [device %d]", __CLASS__, __FUNCTION__, deviceM->GetId())
#define log1(lvl, a) \
   if (logLevel & lvl) \
      debug("%s::%s" a " [device %d]", __CLASS__, __FUNCTION__, deviceM->GetId())
#define log2(lvl, a, b...) \
   if (logLevel & lvl) \
      debug("%s::%s " a " [device %d]", __CLASS__, __FUNCTION__, b, deviceM->GetId())
#define __CLASS__ "cSatipTunerDataThread"

cSatipTunerDataThread::cSatipTunerDataThread(cSatipDeviceIf &deviceP, cSatipTunerStatistics &statisticsP, unsigned int packetLenP)
: cThread("SAT>IP data"),
  deviceM(&deviceP),
  statisticsM(&statisticsP),
  packetBufferLenM(packetLenP),
  packetBufferM(NULL),
  rtpSocketM(NULL),
  timeoutM(-1),
  timeoutHandlerM(0),
  timeoutFuncM(NULL),
  timeoutParamM(NULL),
  sleepM(),
  mutexM()
{
  log2(logFunc, "(...,...,%d)", packetLenP);

  // Allocate packet buffer
  packetBufferM = MALLOC(unsigned char, packetBufferLenM);
  if (packetBufferM)
     memset(packetBufferM, 0, packetBufferLenM);
  else
     error("MALLOC() failed for packet buffer [device %d]", deviceM->GetId());
}

cSatipTunerDataThread::~cSatipTunerDataThread()
{
  log(logFunc);
  cMutexLock MutexLock(&mutexM);

  // Free allocated memory
  free(packetBufferM);
  packetBufferM = NULL;
}

void cSatipTunerDataThread::Start(cSatipSocket *rtpSocketP)
{
  log1(logFunc, "(...)");
  cMutexLock MutexLock(&mutexM);

  rtpSocketM = rtpSocketP;

  // Start thread
  cThread::Start();
}

void cSatipTunerDataThread::SetTimeout(int timeoutP, fCallback callbackP, void *paramP)
{
  log2(logFunc, "(%d, ...)", timeoutP);
  cMutexLock MutexLock(&mutexM);

  if (timeoutP > 0) {
     timeoutM = timeoutP;
     timeoutFuncM = callbackP;
     timeoutParamM = paramP;
     timeoutHandlerM.Set(timeoutM);
     }
  else {
     timeoutM = -1;
     timeoutFuncM = NULL;
     timeoutParamM = NULL;
     timeoutHandlerM.Set(0);
     }
}

void cSatipTunerDataThread::Cancel(int WaitSeconds)
{
  if (Running())
     cThread::Cancel(WaitSeconds);
}

void cSatipTunerDataThread::Flush(void)
{
  log(logFunc);
  cMutexLock MutexLock(&mutexM);

  if (rtpSocketM)
     rtpSocketM->Flush();
}

void cSatipTunerDataThread::Action(void)
{
  log(logFunc);

  // Increase priority
  SetPriority(-1);

  // Do the thread loop
  while (Running() && packetBufferM) {
        int length = -1;
        unsigned int size = min(deviceM->CheckData(), packetBufferLenM);
 
        mutexM.Lock();

        // Read data
        if (rtpSocketM && rtpSocketM->IsOpen()) {
           length = rtpSocketM->ReadVideo(packetBufferM, size);
           log2(logData, "received %d bytes", length);
           }

        if (length > 0) {
           log2(logDataDetails, "trying to write %d bytes", length);
           deviceM->WriteData(packetBufferM, length);
           log2(logDataDetails, "wrote %d bytes", length);

           if (statisticsM)
              statisticsM->AddTunerStatistic(length);

           timeoutHandlerM.Set(timeoutM);
           }

        if (timeoutM > 0 && timeoutFuncM && timeoutHandlerM.TimedOut()) {
           error("No Data received for %d ms [device %d], timeout handling started", timeoutM, deviceM->GetId());
           (*timeoutFuncM)(timeoutParamM);
           timeoutHandlerM.Set(timeoutM);
           }

        mutexM.Unlock();

        // to avoid busy loop and reduce cpu load
        if (length <= 0)
           sleepM.Wait(10);
        }
}
