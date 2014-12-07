/*
 * statistics.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <limits.h>

#include "common.h"
#include "statistics.h"
#include "log.h"
#include "config.h"

// Section statistics class
cSatipSectionStatistics::cSatipSectionStatistics()
: filteredDataM(0),
  numberOfCallsM(0),
  timerM(),
  mutexM()
{
  debug8("%s", __PRETTY_FUNCTION__);
}

cSatipSectionStatistics::~cSatipSectionStatistics()
{
  debug8("%s", __PRETTY_FUNCTION__);
}

cString cSatipSectionStatistics::GetSectionStatistic()
{
  debug8("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  uint64_t elapsed = timerM.Elapsed(); /* in milliseconds */
  timerM.Set();
  long bitrate = elapsed ? (long)(1000.0L * filteredDataM / KILOBYTE(1) / elapsed) : 0L;
  if (!SatipConfig.GetUseBytes())
     bitrate *= 8;
  // no trailing linefeed here!
  cString s = cString::sprintf("%4ld (%4ld k%s/s)", numberOfCallsM, bitrate,
                               SatipConfig.GetUseBytes() ? "B" : "bit");
  filteredDataM = numberOfCallsM = 0;
  return s;
}

void cSatipSectionStatistics::AddSectionStatistic(long bytesP, long callsP)
{
  debug8("%s (%ld, %ld)", __PRETTY_FUNCTION__, bytesP, callsP);
  cMutexLock MutexLock(&mutexM);
  filteredDataM += bytesP;
  numberOfCallsM += callsP;
}

// --- cSatipPidStatistics ----------------------------------------------------

// Device statistics class
cSatipPidStatistics::cSatipPidStatistics()
: timerM(),
  mutexM()
{
  debug1("%s", __PRETTY_FUNCTION__);
  const int numberOfElements = sizeof(mostActivePidsM) / sizeof(pidStruct);
  for (int i = 0; i < numberOfElements; ++i) {
      mostActivePidsM[i].pid = -1;
      mostActivePidsM[i].dataAmount = 0L;
      }
}

cSatipPidStatistics::~cSatipPidStatistics()
{
  debug1("%s", __PRETTY_FUNCTION__);
}

cString cSatipPidStatistics::GetPidStatistic()
{
  debug8("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  const int numberOfElements = sizeof(mostActivePidsM) / sizeof(pidStruct);
  uint64_t elapsed = timerM.Elapsed(); /* in milliseconds */
  timerM.Set();
  cString s("Active pids:\n");
  for (int i = 0; i < numberOfElements; ++i) {
      if (mostActivePidsM[i].pid >= 0) {
         long bitrate = elapsed ? (long)(1000.0L * mostActivePidsM[i].dataAmount / KILOBYTE(1) / elapsed) : 0L;
         if (!SatipConfig.GetUseBytes())
            bitrate *= 8;
         s = cString::sprintf("%sPid %d: %4d (%4ld k%s/s)\n", *s, i,
                              mostActivePidsM[i].pid, bitrate,
                              SatipConfig.GetUseBytes() ? "B" : "bit");
         }
      }
  for (int i = 0; i < numberOfElements; ++i) {
      mostActivePidsM[i].pid = -1;
      mostActivePidsM[i].dataAmount = 0L;
      }
  return s;
}

int cSatipPidStatistics::SortPids(const void* data1P, const void* data2P)
{
  debug8("%s", __PRETTY_FUNCTION__);
  const pidStruct *comp1 = reinterpret_cast<const pidStruct*>(data1P);
  const pidStruct *comp2 = reinterpret_cast<const pidStruct*>(data2P);
  if (comp1->dataAmount > comp2->dataAmount)
     return -1;
  if (comp1->dataAmount < comp2->dataAmount)
     return 1;
  return 0;
}

void cSatipPidStatistics::AddPidStatistic(int pidP, long payloadP)
{
  debug8("%s (%d, %ld)", __PRETTY_FUNCTION__, pidP, payloadP);
  cMutexLock MutexLock(&mutexM);
  const int numberOfElements = sizeof(mostActivePidsM) / sizeof(pidStruct);
  // If our statistic already is in the array, update it and quit
  for (int i = 0; i < numberOfElements; ++i) {
      if (mostActivePidsM[i].pid == pidP) {
         mostActivePidsM[i].dataAmount += payloadP;
         // Now re-sort the array and quit
         qsort(mostActivePidsM, numberOfElements, sizeof(pidStruct), SortPids);
         return;
         }
      }
  // Apparently our pid isn't in the array. Replace the last element with this
  // one if new payload is greater
  if (mostActivePidsM[numberOfElements - 1].dataAmount < payloadP) {
      mostActivePidsM[numberOfElements - 1].pid = pidP;
      mostActivePidsM[numberOfElements - 1].dataAmount = payloadP;
     // Re-sort
     qsort(mostActivePidsM, numberOfElements, sizeof(pidStruct), SortPids);
     }
}

// --- cSatipTunerStatistics --------------------------------------------------

// Tuner statistics class
cSatipTunerStatistics::cSatipTunerStatistics()
: dataBytesM(0),
  timerM(),
  mutexM()
{
  debug1("%s", __PRETTY_FUNCTION__);
}

cSatipTunerStatistics::~cSatipTunerStatistics()
{
  debug1("%s", __PRETTY_FUNCTION__);
}

cString cSatipTunerStatistics::GetTunerStatistic()
{
  debug8("%s", __PRETTY_FUNCTION__);
  mutexM.Lock();
  uint64_t elapsed = timerM.Elapsed(); /* in milliseconds */
  timerM.Set();
  long bitrate = elapsed ? (long)(1000.0L * dataBytesM / KILOBYTE(1) / elapsed) : 0L;
  dataBytesM = 0;
  mutexM.Unlock();

  if (!SatipConfig.GetUseBytes())
     bitrate *= 8;
  cString s = cString::sprintf("%ld k%s/s", bitrate, SatipConfig.GetUseBytes() ? "B" : "bit");
  return s;
}

void cSatipTunerStatistics::AddTunerStatistic(long bytesP)
{
  debug8("%s (%ld)", __PRETTY_FUNCTION__, bytesP);
  cMutexLock MutexLock(&mutexM);
  dataBytesM += bytesP;
}


// Buffer statistics class
cSatipBufferStatistics::cSatipBufferStatistics()
: dataBytesM(0),
  freeSpaceM(0),
  usedSpaceM(0),
  timerM(),
  mutexM()
{
  debug1("%s", __PRETTY_FUNCTION__);
}

cSatipBufferStatistics::~cSatipBufferStatistics()
{
  debug1("%s", __PRETTY_FUNCTION__);
}

cString cSatipBufferStatistics::GetBufferStatistic()
{
  debug8("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  uint64_t elapsed = timerM.Elapsed(); /* in milliseconds */
  timerM.Set();
  long bitrate = elapsed ? (long)(1000.0L * dataBytesM / KILOBYTE(1) / elapsed) : 0L;
  long totalSpace = SATIP_BUFFER_SIZE;
  float percentage = (float)((float)usedSpaceM / (float)totalSpace * 100.0);
  long totalKilos = totalSpace / KILOBYTE(1);
  long usedKilos = usedSpaceM / KILOBYTE(1);
  if (!SatipConfig.GetUseBytes()) {
     bitrate *= 8;
     totalKilos *= 8;
     usedKilos *= 8;
     }
  cString s = cString::sprintf("Buffer bitrate: %ld k%s/s\nBuffer usage: %ld/%ld k%s (%2.1f%%)\n", bitrate,
                               SatipConfig.GetUseBytes() ? "B" : "bit", usedKilos, totalKilos,
                               SatipConfig.GetUseBytes() ? "B" : "bit", percentage);
  dataBytesM = 0;
  usedSpaceM = 0;
  return s;
}

void cSatipBufferStatistics::AddBufferStatistic(long bytesP, long usedP)
{
  debug8("%s (%ld, %ld)", __PRETTY_FUNCTION__, bytesP, usedP);
  cMutexLock MutexLock(&mutexM);
  dataBytesM += bytesP;
  if (usedP > usedSpaceM)
     usedSpaceM = usedP;
}
