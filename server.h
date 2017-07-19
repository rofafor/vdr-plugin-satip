/*
 * server.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_SERVER_H
#define __SATIP_SERVER_H

class cSatipServer;

// --- cSatipFrontend ---------------------------------------------------------

class cSatipFrontend : public cListObject {
private:
  int indexM;
  int transponderM;
  int deviceIdM;
  cString descriptionM;

public:
  cSatipFrontend(const int indexP, const char *descriptionP);
  virtual ~cSatipFrontend();
  void Attach(int deviceIdP) { deviceIdM = deviceIdP; }
  void Detach(int deviceIdP) { if (deviceIdP == deviceIdM) deviceIdM = -1; }
  cString Description(void) { return descriptionM; }
  bool Attached(void) { return (deviceIdM >= 0); }
  int Index(void) { return indexM; }
  int Transponder(void) { return transponderM; }
  int DeviceId(void) { return deviceIdM; }
  void SetTransponder(int transponderP) { transponderM = transponderP; }
};

// --- cSatipFrontends --------------------------------------------------------

class cSatipFrontends : public cList<cSatipFrontend> {
public:
  bool Matches(int deviceIdP, int transponderP);
  bool Assign(int deviceIdP, int transponderP);
  bool Attach(int deviceIdP, int transponderP);
  bool Detach(int deviceIdP, int transponderP);
};

// --- cSatipServer -----------------------------------------------------------

class cSatipServer : public cListObject {
private:
  enum eSatipFrontend {
    eSatipFrontendDVBS2 = 0,
    eSatipFrontendDVBT,
    eSatipFrontendDVBT2,
    eSatipFrontendDVBC,
    eSatipFrontendDVBC2,
    eSatipFrontendCount
  };
  enum {
    eSatipMaxSourceFilters = 16
  };
  cString srcAddressM;
  cString addressM;
  cString modelM;
  cString filtersM;
  cString descriptionM;
  cString quirksM;
  cSatipFrontends frontendsM[eSatipFrontendCount];
  int sourceFiltersM[eSatipMaxSourceFilters];
  int portM;
  int quirkM;
  bool hasCiM;
  bool activeM;
  time_t createdM;
  cTimeMs lastSeenM;
  bool IsValidSource(int sourceP);

public:
  enum eSatipQuirk {
    eSatipQuirkNone       = 0x00,
    eSatipQuirkSessionId  = 0x01,
    eSatipQuirkPlayPids   = 0x02,
    eSatipQuirkForceLock  = 0x04,
    eSatipQuirkRtpOverTcp = 0x08,
    eSatipQuirkCiXpmt     = 0x10,
    eSatipQuirkCiTnr      = 0x20,
    eSatipQuirkForcePilot = 0x40,
    eSatipQuirkMask       = 0xFF
  };
  cSatipServer(const char *srcAddressP, const char *addressP, const int portP, const char *modelP, const char *filtersP, const char *descriptionP, const int quirkP);
  virtual ~cSatipServer();
  virtual int Compare(const cListObject &listObjectP) const;
  bool Assign(int deviceIdP, int sourceP, int systemP, int transponderP);
  bool Matches(int sourceP);
  bool Matches(int deviceIdP, int sourceP, int systemP, int transponderP);
  void Attach(int deviceIdP, int transponderP);
  void Detach(int deviceIdP, int transponderP);
  int GetModulesDVBS2(void);
  int GetModulesDVBT(void);
  int GetModulesDVBT2(void);
  int GetModulesDVBC(void);
  int GetModulesDVBC2(void);
  void Activate(bool onOffP)    { activeM = onOffP; }
  const char *SrcAddress(void)  { return *srcAddressM; }
  const char *Address(void)     { return *addressM; }
  const char *Model(void)       { return *modelM; }
  const char *Filters(void)     { return *filtersM; }
  const char *Description(void) { return *descriptionM; }
  const char *Quirks(void)      { return *quirksM; }
  int Port(void)                { return portM; }
  bool Quirk(int quirkP)        { return ((quirkP & eSatipQuirkMask) & quirkM); }
  bool HasQuirk(void)           { return (quirkM != eSatipQuirkNone); }
  bool HasCI(void)              { return hasCiM; }
  bool IsActive(void)           { return activeM; }
  void Update(void)             { lastSeenM.Set(); }
  uint64_t LastSeen(void)       { return lastSeenM.Elapsed(); }
  time_t Created(void)          { return createdM; }
};

// --- cSatipServers ----------------------------------------------------------

class cSatipServers : public cList<cSatipServer> {
public:
  cSatipServer *Find(cSatipServer *serverP);
  cSatipServer *Find(int sourceP);
  cSatipServer *Assign(int deviceIdP, int sourceP, int transponderP, int systemP);
  cSatipServer *Update(cSatipServer *serverP);
  void Activate(cSatipServer *serverP, bool onOffP);
  void Attach(cSatipServer *serverP, int deviceIdP, int transponderP);
  void Detach(cSatipServer *serverP, int deviceIdP, int transponderP);
  bool IsQuirk(cSatipServer *serverP, int quirkP);
  bool HasCI(cSatipServer *serverP);
  void Cleanup(uint64_t intervalMsP = 0);
  cString GetAddress(cSatipServer *serverP);
  cString GetSrcAddress(cSatipServer *serverP);
  cString GetString(cSatipServer *serverP);
  int GetPort(cSatipServer *serverP);
  cString List(void);
  int NumProvidedSystems(void);
};

#endif // __SATIP_SERVER_H
