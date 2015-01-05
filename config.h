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
  unsigned int traceModeM;
  unsigned int eitScanM;
  unsigned int useBytesM;
  int disabledSourcesM[MAX_DISABLED_SOURCES_COUNT];
  int disabledFiltersM[SECTION_FILTER_TABLE_SIZE];

public:
  enum eOperatingMode {
    eOperatingModeOff = 0,
    eOperatingModeLow,
    eOperatingModeNormal,
    eOperatingModeHigh,
    eOperatingModeCount
  };
  enum eTraceMode {
   eTraceModeNormal  = 0x0000,
   eTraceModeDebug1  = 0x0001,
   eTraceModeDebug2  = 0x0002,
   eTraceModeDebug3  = 0x0004,
   eTraceModeDebug4  = 0x0008,
   eTraceModeDebug5  = 0x0010,
   eTraceModeDebug6  = 0x0020,
   eTraceModeDebug7  = 0x0040,
   eTraceModeDebug8  = 0x0080,
   eTraceModeDebug9  = 0x0100,
   eTraceModeDebug10 = 0x0200,
   eTraceModeDebug11 = 0x0400,
   eTraceModeDebug12 = 0x0800,
   eTraceModeDebug13 = 0x1000,
   eTraceModeDebug14 = 0x2000,
   eTraceModeDebug15 = 0x4000,
   eTraceModeDebug16 = 0x8000,
   eTraceModeMask    = 0xFFFF
  };
  cSatipConfig();
  unsigned int GetOperatingMode(void) const { return operatingModeM; }
  bool IsOperatingModeOff(void) const { return (operatingModeM == eOperatingModeOff); }
  bool IsOperatingModeLow(void) const { return (operatingModeM == eOperatingModeLow); }
  bool IsOperatingModeNormal(void) const { return (operatingModeM == eOperatingModeNormal); }
  bool IsOperatingModeHigh(void) const { return (operatingModeM == eOperatingModeHigh); }
  void ToggleOperatingMode(void) { operatingModeM = (operatingModeM + 1) % eOperatingModeCount; }
  unsigned int GetTraceMode(void) const { return traceModeM; }
  bool IsTraceMode(eTraceMode modeP) const { return (traceModeM & modeP); }
  unsigned int GetEITScan(void) const { return eitScanM; }
  unsigned int GetUseBytes(void) const { return useBytesM; }
  unsigned int GetDisabledSourcesCount(void) const;
  int GetDisabledSources(unsigned int indexP) const;
  unsigned int GetDisabledFiltersCount(void) const;
  int GetDisabledFilters(unsigned int indexP) const;

  void SetOperatingMode(unsigned int operatingModeP) { operatingModeM = operatingModeP; }
  void SetTraceMode(unsigned int modeP) { traceModeM = (modeP & eTraceModeMask); }
  void SetEITScan(unsigned int onOffP) { eitScanM = onOffP; }
  void SetUseBytes(unsigned int onOffP) { useBytesM = onOffP; }
  void SetDisabledSources(unsigned int indexP, int sourceP);
  void SetDisabledFilters(unsigned int indexP, int numberP);
};

extern cSatipConfig SatipConfig;

#endif // __SATIP_CONFIG_H
