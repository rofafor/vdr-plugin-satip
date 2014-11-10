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

int logLevel = logFunc /*| logFuncPerf | logData*/;

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
: cThread(cString::sprintf("SAT>IP data %d", deviceP.GetId())),
  deviceM(&deviceP),
  statisticsM(&statisticsP),
  packetBufferLenM(packetLenP),
  packetBufferM(NULL),
  rtpSocketM(NULL),
  timeDataReceivedM(),
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

int cSatipTunerDataThread::LastReceivedMs()
{
  int rc = timeDataReceivedM.Elapsed();

  log2(logFuncPerf, "returning %d", rc);

  return rc;
}

void cSatipTunerDataThread::ResetLastReceivedMs()
{
  log(logFunc);

  timeDataReceivedM.Set();
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
  bool polling = SatipConfig.IsPolling();

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
           if (!polling || length > 0)
              timeDataReceivedM.Set();

           log2(logData, "received %d bytes", length);
           }

        if (length > 0) {
           log2(logDataDetails, "trying to write %d bytes", length);
           deviceM->WriteData(packetBufferM, length);
           log2(logDataDetails, "wrote %d bytes", length);

           if (statisticsM)
              statisticsM->AddTunerStatistic(length);

           }

        mutexM.Unlock();

        // to avoid busy loop and reduce cpu load
        if (polling && length <= 0)
           sleepM.Wait(10);
        }
}
