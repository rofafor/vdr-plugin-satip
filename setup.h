/*
 * setup.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_SETUP_H
#define __SATIP_SETUP_H

#include <vdr/menuitems.h>
#include <vdr/sourceparams.h>
#include "common.h"

class cSatipPluginSetup : public cMenuSetupPage
{
private:
  int deviceCountM;
  int operatingModeM;
  const char *operatingModeTextsM[cSatipConfig::eOperatingModeCount];
  int eitScanM;
  int numDisabledFiltersM;
  int disabledFilterIndexesM[SECTION_FILTER_TABLE_SIZE];
  const char *disabledFilterNamesM[SECTION_FILTER_TABLE_SIZE];
  cVector<const char*> helpM;

  eOSState DeviceScan(void);
  eOSState DeviceInfo(void);
  eOSState ShowInfo(void);
  void Setup(void);
  void StoreFilters(const char *nameP, int *valuesP);

protected:
  virtual eOSState ProcessKey(eKeys keyP);
  virtual void Store(void);

public:
  cSatipPluginSetup();
};

#endif // __SATIP_SETUP_H
