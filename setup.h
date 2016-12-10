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
  bool detachedModeM;
  int deviceCountM;
  int operatingModeM;
  int transportModeM;
  const char *operatingModeTextsM[cSatipConfig::eOperatingModeCount];
  const char *transportModeTextsM[cSatipConfig::eTransportModeCount];
  int ciExtensionM;
  int cicamsM[MAX_CICAM_COUNT];
  const char *cicamTextsM[CA_SYSTEMS_TABLE_SIZE];
  int eitScanM;
  int numDisabledSourcesM;
  int disabledSourcesM[MAX_DISABLED_SOURCES_COUNT];
  int numDisabledFiltersM;
  int disabledFilterIndexesM[SECTION_FILTER_TABLE_SIZE];
  const char *disabledFilterNamesM[SECTION_FILTER_TABLE_SIZE];
  cVector<const char*> helpM;

  eOSState DeviceScan(void);
  eOSState DeviceInfo(void);
  eOSState ShowDeviceStatus(void);
  eOSState ShowInfo(void);
  void Setup(void);
  void StoreCicams(const char *nameP, int *cicamsP);
  void StoreSources(const char *nameP, int *sourcesP);
  void StoreFilters(const char *nameP, int *valuesP);

protected:
  virtual eOSState ProcessKey(eKeys keyP);
  virtual void Store(void);

public:
  cSatipPluginSetup();
};

#endif // __SATIP_SETUP_H
