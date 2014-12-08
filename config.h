/*
 * config.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_CONFIG_H
#define __SATIP_CONFIG_H

#include <vdr/menuitems.h>
#include "common.h"

class cSatipConfig
{
private:
  unsigned int operatingModeM;
  unsigned int loggingModeM;
  unsigned int eitScanM;
  unsigned int useBytesM;
  int disabledSourcesM[MAX_DISABLED_SOURCES_COUNT];
  int disabledFiltersM[SECTION_FILTER_TABLE_SIZE];
  char configDirectoryM[PATH_MAX];

public:
  enum eOperatingMode {
    eOperatingModeOff = 0,
    eOperatingModeLow,
    eOperatingModeNormal,
    eOperatingModeHigh,
    eOperatingModeCount
  };
  enum eLoggingMode {
   eLoggingModeNormal  = 0x0000,
   eLoggingModeDebug1  = 0x0001,
   eLoggingModeDebug2  = 0x0002,
   eLoggingModeDebug3  = 0x0004,
   eLoggingModeDebug4  = 0x0008,
   eLoggingModeDebug5  = 0x0010,
   eLoggingModeDebug6  = 0x0020,
   eLoggingModeDebug7  = 0x0040,
   eLoggingModeDebug8  = 0x0080,
   eLoggingModeDebug9  = 0x0100,
   eLoggingModeDebug10 = 0x0200,
   eLoggingModeDebug11 = 0x0400,
   eLoggingModeDebug12 = 0x0800,
   eLoggingModeDebug13 = 0x1000,
   eLoggingModeDebug14 = 0x2000,
   eLoggingModeDebug15 = 0x4000,
   eLoggingModeDebug16 = 0x8000,
   eLoggingModeMask    = 0xFFFF
  };
  cSatipConfig();
  unsigned int GetOperatingMode(void) const { return operatingModeM; }
  bool IsOperatingModeOff(void) const { return (operatingModeM == eOperatingModeOff); }
  bool IsOperatingModeLow(void) const { return (operatingModeM == eOperatingModeLow); }
  bool IsOperatingModeNormal(void) const { return (operatingModeM == eOperatingModeNormal); }
  bool IsOperatingModeHigh(void) const { return (operatingModeM == eOperatingModeHigh); }
  void ToggleOperatingMode(void) { operatingModeM = (operatingModeM + 1) % eOperatingModeCount; }
  unsigned int GetLoggingMode(void) const { return loggingModeM; }
  bool IsLoggingMode(eLoggingMode modeP) const { return (loggingModeM & modeP); }
  unsigned int GetEITScan(void) const { return eitScanM; }
  unsigned int GetUseBytes(void) const { return useBytesM; }
  const char *GetConfigDirectory(void) const { return configDirectoryM; }
  unsigned int GetDisabledSourcesCount(void) const;
  int GetDisabledSources(unsigned int indexP) const;
  unsigned int GetDisabledFiltersCount(void) const;
  int GetDisabledFilters(unsigned int indexP) const;

  void SetOperatingMode(unsigned int operatingModeP) { operatingModeM = operatingModeP; }
  void SetLoggingMode(unsigned int modeP) { loggingModeM = (modeP & eLoggingModeMask); }
  void SetEITScan(unsigned int onOffP) { eitScanM = onOffP; }
  void SetUseBytes(unsigned int onOffP) { useBytesM = onOffP; }
  void SetConfigDirectory(const char *directoryP);
  void SetDisabledSources(unsigned int indexP, int sourceP);
  void SetDisabledFilters(unsigned int indexP, int numberP);
};

extern cSatipConfig SatipConfig;

#endif // __SATIP_CONFIG_H
