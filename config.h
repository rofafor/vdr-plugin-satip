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
  unsigned int eitScanM;
  unsigned int useBytesM;
  int disabledFiltersM[SECTION_FILTER_TABLE_SIZE];
  char configDirectoryM[PATH_MAX];

public:
  enum {
    OPERATING_MODE_OFF = 0,
    OPERATING_MODE_LOW,
    OPERATING_MODE_NORMAL,
    OPERATING_MODE_HIGH,
    NUMBER_OF_OPERATING_MODES
  };
  cSatipConfig();
  unsigned int GetOperatingMode(void) const { return operatingModeM; }
  bool IsOperatingModeOff(void) const { return (operatingModeM == OPERATING_MODE_OFF); }
  bool IsOperatingModeLow(void) const { return (operatingModeM == OPERATING_MODE_LOW); }
  bool IsOperatingModeNormal(void) const { return (operatingModeM == OPERATING_MODE_NORMAL); }
  bool IsOperatingModeHigh(void) const { return (operatingModeM == OPERATING_MODE_HIGH); }
  void ToggleOperatingMode(void) { operatingModeM = (operatingModeM + 1) % NUMBER_OF_OPERATING_MODES; }
  unsigned int GetEITScan(void) const { return eitScanM; }
  unsigned int GetUseBytes(void) const { return useBytesM; }
  const char *GetConfigDirectory(void) const { return configDirectoryM; }
  unsigned int GetDisabledFiltersCount(void) const;
  int GetDisabledFilters(unsigned int indexP) const;

  void SetOperatingMode(unsigned int operatingModeP) { operatingModeM = operatingModeP; }
  void SetEITScan(unsigned int onOffP) { eitScanM = onOffP; }
  void SetUseBytes(unsigned int onOffP) { useBytesM = onOffP; }
  void SetConfigDirectory(const char *directoryP);
  void SetDisabledFilters(unsigned int indexP, int numberP);
};

extern cSatipConfig SatipConfig;

#endif // __SATIP_CONFIG_H
