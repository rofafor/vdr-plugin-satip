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
  unsigned int ciExtensionM;
  unsigned int eitScanM;
  unsigned int useBytesM;
  unsigned int portRangeStartM;
  unsigned int portRangeStopM;
  unsigned int transportModeM;
  bool detachedModeM;
  bool disableServerQuirksM;
  bool useSingleModelServersM;
  int cicamsM[MAX_CICAM_COUNT];
  int disabledSourcesM[MAX_DISABLED_SOURCES_COUNT];
  int disabledFiltersM[SECTION_FILTER_TABLE_SIZE];
  size_t rtpRcvBufSizeM;

public:
  enum eOperatingMode {
    eOperatingModeOff = 0,
    eOperatingModeLow,
    eOperatingModeNormal,
    eOperatingModeHigh,
    eOperatingModeCount
  };
  enum eTransportMode {
    eTransportModeUnicast = 0,
    eTransportModeMulticast,
    eTransportModeRtpOverTcp,
    eTransportModeCount
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
  unsigned int GetCIExtension(void) const { return ciExtensionM; }
  int GetCICAM(unsigned int indexP) const;
  unsigned int GetEITScan(void) const { return eitScanM; }
  unsigned int GetUseBytes(void) const { return useBytesM; }
  unsigned int GetTransportMode(void) const { return transportModeM; }
  bool IsTransportModeUnicast(void) const { return (transportModeM == eTransportModeUnicast); }
  bool IsTransportModeRtpOverTcp(void) const { return (transportModeM == eTransportModeRtpOverTcp); }
  bool IsTransportModeMulticast(void) const { return (transportModeM == eTransportModeMulticast); }
  bool GetDetachedMode(void) const { return detachedModeM; }
  bool GetDisableServerQuirks(void) const { return disableServerQuirksM; }
  bool GetUseSingleModelServers(void) const { return useSingleModelServersM; }
  unsigned int GetDisabledSourcesCount(void) const;
  int GetDisabledSources(unsigned int indexP) const;
  unsigned int GetDisabledFiltersCount(void) const;
  int GetDisabledFilters(unsigned int indexP) const;
  unsigned int GetPortRangeStart(void) const { return portRangeStartM; }
  unsigned int GetPortRangeStop(void) const { return portRangeStopM; }
  size_t GetRtpRcvBufSize(void) const { return rtpRcvBufSizeM; }

  void SetOperatingMode(unsigned int operatingModeP) { operatingModeM = operatingModeP; }
  void SetTraceMode(unsigned int modeP) { traceModeM = (modeP & eTraceModeMask); }
  void SetCIExtension(unsigned int onOffP) { ciExtensionM = onOffP; }
  void SetCICAM(unsigned int indexP, int cicamP);
  void SetEITScan(unsigned int onOffP) { eitScanM = onOffP; }
  void SetUseBytes(unsigned int onOffP) { useBytesM = onOffP; }
  void SetTransportMode(unsigned int transportModeP) { transportModeM = transportModeP; }
  void SetDetachedMode(bool onOffP) { detachedModeM = onOffP; }
  void SetDisableServerQuirks(bool onOffP) { disableServerQuirksM = onOffP; }
  void SetUseSingleModelServers(bool onOffP) { useSingleModelServersM = onOffP; }
  void SetDisabledSources(unsigned int indexP, int sourceP);
  void SetDisabledFilters(unsigned int indexP, int numberP);
  void SetPortRangeStart(unsigned int rangeStartP) { portRangeStartM = rangeStartP; }
  void SetPortRangeStop(unsigned int rangeStopP) { portRangeStopM = rangeStopP; }
  void SetRtpRcvBufSize(size_t sizeP) { rtpRcvBufSizeM = sizeP; }
};

extern cSatipConfig SatipConfig;

#endif // __SATIP_CONFIG_H
