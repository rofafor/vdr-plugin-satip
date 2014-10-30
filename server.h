/*
 * server.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_SERVER_H
#define __SATIP_SERVER_H

// --- cSatipServer -----------------------------------------------------------

class cSatipServer : public cListObject {
private:
  enum eSatipModule {
    eSatipModuleDVBS2 = 0,
    eSatipModuleDVBT,
    eSatipModuleDVBT2,
    eSatipModuleDVBC,
    eSatipModuleCount
  };
  cString addressM;
  cString descriptionM;
  cString modelM;
  int modelCountM[eSatipModuleCount];
  int modelTypeM;
  int quirkM;
  int useCountM;
  time_t createdM;
  cTimeMs lastSeenM;

public:
  enum eSatipQuirk {
    eSatipQuirkNone      = 0x00,
    eSatipQuirkSessionId = 0x01,
    eSatipQuirkMask      = 0x0F
  };
  enum eSatipModelType {
    eSatipModelTypeNone  = 0x00,
    eSatipModelTypeDVBS2 = 0x01,
    eSatipModelTypeDVBT  = 0x02,
    eSatipModelTypeDVBT2 = 0x04,
    eSatipModelTypeDVBC  = 0x08,
    eSatipModelTypeMask  = 0x0F
  };
  cSatipServer(const char *addressP, const char *descriptionP, const char *modelP);
  virtual ~cSatipServer();
  virtual int Compare(const cListObject &listObjectP) const;
  void Use(bool onOffP);
  bool Used(void)           { return !!useCountM; }
  const char *Description() { return *descriptionM; }
  const char *Address()     { return *addressM; }
  const char *Model(void)   { return modelM; }
  bool Quirk(int quirkP)    { return ((quirkP & eSatipQuirkMask) & quirkM); }
  int ModelType(void)       { return modelTypeM; }
  bool Match(int modelP)    { return ((modelP & eSatipModelTypeMask) & modelTypeM); }
  int Cable()               { return Match(eSatipModelTypeDVBC)  ? modelCountM[eSatipModuleDVBC] : 0; }
  int Satellite()           { return Match(eSatipModelTypeDVBS2) ? modelCountM[eSatipModuleDVBS2] : 0; }
  int Terrestrial()         { return Match(eSatipModelTypeDVBT)  ? modelCountM[eSatipModuleDVBT]  : 0; }
  int Terrestrial2()        { return Match(eSatipModelTypeDVBT2) ? modelCountM[eSatipModuleDVBT2] : 0; }
  void Update(void)         { lastSeenM.Set(); }
  uint64_t LastSeen(void)   { return lastSeenM.Elapsed(); }
  time_t Created(void)      { return createdM; }
};

// --- cSatipServers ----------------------------------------------------------

class cSatipServers : public cList<cSatipServer> {
public:
  cSatipServer *Find(cSatipServer *serverP);
  cSatipServer *Find(int sourceP, int systemP);
  cSatipServer *Update(cSatipServer *serverP);
  void Use(cSatipServer *serverP, bool onOffP);
  void Cleanup(uint64_t intervalMsP = 0);
  cString GetString(cSatipServer *serverP);
  cString List(void);
  int NumProvidedSystems(void);
};

#endif // __SATIP_SERVER_H
