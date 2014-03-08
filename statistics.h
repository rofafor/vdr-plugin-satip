/*
 * statistics.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_STATISTICS_H
#define __SATIP_STATISTICS_H

#include <vdr/thread.h>

// Section statistics
class cSatipSectionStatistics {
public:
  cSatipSectionStatistics();
  virtual ~cSatipSectionStatistics();
  cString GetSectionStatistic();

protected:
  void AddSectionStatistic(long bytesP, long callsP);

private:
  long filteredDataM;
  long numberOfCallsM;
  cTimeMs timerM;
  cMutex mutexM;
};

// Pid statistics
class cSatipPidStatistics {
public:
  cSatipPidStatistics();
  virtual ~cSatipPidStatistics();
  cString GetPidStatistic();

protected:
  void AddPidStatistic(int pidP, long payloadP);

private:
  struct pidStruct {
    int  pid;
    long dataAmount;
  };
  pidStruct mostActivePidsM[SATIP_STATS_ACTIVE_PIDS_COUNT];
  cTimeMs timerM;
  cMutex mutexM;

private:
  static int SortPids(const void* data1P, const void* data2P);
};

// Tuner statistics
class cSatipTunerStatistics {
public:
  cSatipTunerStatistics();
  virtual ~cSatipTunerStatistics();
  cString GetTunerStatistic();

protected:
  void AddTunerStatistic(long bytesP);

private:
  long dataBytesM;
  cTimeMs timerM;
  cMutex mutexM;
};

// Buffer statistics
class cSatipBufferStatistics {
public:
  cSatipBufferStatistics();
  virtual ~cSatipBufferStatistics();
  cString GetBufferStatistic();

protected:
  void AddBufferStatistic(long bytesP, long usedP);

private:
  long dataBytesM;
  long freeSpaceM;
  long usedSpaceM;
  cTimeMs timerM;
  cMutex mutexM;
};

#endif // __SATIP_STATISTICS_H
