/*
 * source.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_SOURCE_H
#define __SATIP_SOURCE_H

#include <vdr/menuitems.h>
#include <vdr/sourceparams.h>
#include "common.h"

struct tSatipParameterMap {
  int userValue;
  const char *userString;
  const char *satipString;
};

class cSatipTransponderParameters {
  friend class cSatipSourceParam;

private:
  char polarizationM;
  int bandwidthM;
  int coderateHM;
  int systemM;
  int modulationM;
  int transmissionM;
  int guardM;
  int rollOffM;
  int streamIdM;
  int t2SystemIdM;
  int sisoMisoM;
  int pilotTonesM;
  int signalSourceM;
  int PrintParameter(char *ptrP, char nameP, int valueP) const;
  int PrintString(char *ptrP, int valueP, const tSatipParameterMap *mapP);
  const char *ParseParameter(const char *strP, int &valueP);

public:
  cSatipTransponderParameters(const char *parametersP = NULL);
  char Polarization(void) const { return polarizationM; }
  int Bandwidth(void) const { return bandwidthM; }
  int CoderateH(void) const { return coderateHM; }
  int System(void) const { return systemM; }
  int Modulation(void) const { return modulationM; }
  int Transmission(void) const { return transmissionM; }
  int Guard(void) const { return guardM; }
  int RollOff(void) const { return rollOffM; }
  int StreamId(void) const { return streamIdM; }
  int T2SystemId(void) const { return t2SystemIdM; }
  int SisoMiso(void) const { return sisoMisoM; }
  int PilotTones(void) const { return pilotTonesM; }
  int SignalSource(void) const { return signalSourceM; }
  void SetPolarization(char polarizationP) { polarizationM = polarizationP; }
  void SetBandwidth(int bandwidthP) { bandwidthM = bandwidthP; }
  void SetCoderateH(int coderateHP) { coderateHM = coderateHP; }
  void SetSystem(int systemP) { systemM = systemP; }
  void SetModulation(int modulationP) { modulationM = modulationP; }
  void SetTransmission(int transmissionP) { transmissionM = transmissionP; }
  void SetGuard(int guardP) { guardM = guardP; }
  void SetRollOff(int rollOffP) { rollOffM = rollOffP; }
  void SetStreamId(int streamIdP) { streamIdM = streamIdP; }
  void SetT2SystemId(int t2SystemIdP) { t2SystemIdM = t2SystemIdP; }
  void SetSisoMiso(int sisoMisoP) { sisoMisoM = sisoMisoP; }
  void SetPilotTones(int pilotTonesP) { pilotTonesM = pilotTonesP; }
  void SetSignalSource(int signalSourceP) { signalSourceM = signalSourceP; }
  cString UrlParameters(char typeP);
  cString ToString(char typeP) const;
  bool Parse(const char *strP);
};

class cSatipSourceParam : public cSourceParam
{
private:
  int paramM;
  int nidM;
  int tidM;
  int ridM;
  int srateM;
  cChannel dataM;
  cSatipTransponderParameters stpM;

public:
  cSatipSourceParam(char sourceP, const char *descriptionP);
  virtual void SetData(cChannel *channelP);
  virtual void GetData(cChannel *channelP);
  virtual cOsdItem *GetOsdItem(void);
};

#endif // __SATIP_SOURCE_H
