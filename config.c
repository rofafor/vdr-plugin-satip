/*
 * config.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "discover.h"
#include "log.h"
#include "config.h"

cSatipConfig SatipConfig;

cSatipConfig::cSatipConfig(void)
: operatingModeM(eOperatingModeLow),
  traceModeM(eTraceModeNormal),
  ciExtensionM(0),
  eitScanM(1),
  useBytesM(1),
  portRangeStartM(0),
  portRangeStopM(0),
  transportModeM(eTransportModeUnicast),
  detachedModeM(false),
  disableServerQuirksM(false),
  useSingleModelServersM(false),
  rtpRcvBufSizeM(0)
{
  for (unsigned int i = 0; i < ELEMENTS(cicamsM); ++i)
      cicamsM[i] = 0;
  for (unsigned int i = 0; i < ELEMENTS(disabledSourcesM); ++i)
      disabledSourcesM[i] = cSource::stNone;
  for (unsigned int i = 0; i < ELEMENTS(disabledFiltersM); ++i)
      disabledFiltersM[i] = -1;
}

int cSatipConfig::GetCICAM(unsigned int indexP) const
{
  return (indexP < ELEMENTS(cicamsM)) ? cicamsM[indexP] : -1;
}

void cSatipConfig::SetCICAM(unsigned int indexP, int cicamP)
{
  if (indexP < ELEMENTS(cicamsM))
     cicamsM[indexP] = cicamP;
}

unsigned int cSatipConfig::GetDisabledSourcesCount(void) const
{
  unsigned int n = 0;
  while ((n < ELEMENTS(disabledSourcesM) && (disabledSourcesM[n] != cSource::stNone)))
        n++;
  return n;
}

int cSatipConfig::GetDisabledSources(unsigned int indexP) const
{
  return (indexP < ELEMENTS(disabledSourcesM)) ? disabledSourcesM[indexP] : cSource::stNone;
}

void cSatipConfig::SetDisabledSources(unsigned int indexP, int sourceP)
{
  if (indexP < ELEMENTS(disabledSourcesM))
     disabledSourcesM[indexP] = sourceP;
}

unsigned int cSatipConfig::GetDisabledFiltersCount(void) const
{
  unsigned int n = 0;
  while ((n < ELEMENTS(disabledFiltersM) && (disabledFiltersM[n] != -1)))
        n++;
  return n;
}

int cSatipConfig::GetDisabledFilters(unsigned int indexP) const
{
  return (indexP < ELEMENTS(disabledFiltersM)) ? disabledFiltersM[indexP] : -1;
}

void cSatipConfig::SetDisabledFilters(unsigned int indexP, int numberP)
{
  if (indexP < ELEMENTS(disabledFiltersM))
     disabledFiltersM[indexP] = numberP;
}
