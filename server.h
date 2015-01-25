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
  cString descriptionM;
  bool usedM;

public:
  cSatipFrontend(const int indexP, const char *descriptionP);
  virtual ~cSatipFrontend();
  void Use(bool onOffP) { usedM = onOffP; }
  cString Description(void) { return descriptionM; }
  bool IsUsed(void) { return usedM; }
  int Index(void) { return indexM; }
  int Transponder(void) { return transponderM; }
  void SetTransponder(int transponderP) { transponderM = transponderP; }
};

// --- cSatipFrontends --------------------------------------------------------

class cSatipFrontends : public cList<cSatipFrontend> {
public:
  bool Matches(int transponderP);
  bool Assign(int transponderP);
  bool Use(int transponderP, bool onOffP);
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
  cString addressM;
  cString modelM;
  cString descriptionM;
  cSatipFrontends frontendsM[eSatipFrontendCount];
  int quirkM;
  bool hasCiM;
  time_t createdM;
  cTimeMs lastSeenM;

public:
  enum eSatipQuirk {
    eSatipQuirkNone      = 0x00,
    eSatipQuirkSessionId = 0x01,
    eSatipQuirkPlayPids  = 0x02,
    eSatipQuirkForceLock = 0x04,
    eSatipQuirkMask      = 0x0F
  };
  cSatipServer(const char *addressP, const char *modelP, const char *descriptionP);
  virtual ~cSatipServer();
  virtual int Compare(const cListObject &listObjectP) const;
  bool Assign(int sourceP, int systemP, int transponderP);
  bool Matches(int sourceP);
  bool Matches(int sourceP, int systemP, int transponderP);
  void Use(int transponderP, bool onOffP);
  int GetModulesDVBS2(void);
  int GetModulesDVBT(void);
  int GetModulesDVBT2(void);
  int GetModulesDVBC(void);
  int GetModulesDVBC2(void);
  const char *Address()     { return *addressM; }
  const char *Model(void)   { return *modelM; }
  const char *Description() { return *descriptionM; }
  bool Quirk(int quirkP)    { return ((quirkP & eSatipQuirkMask) & quirkM); }
  bool HasQuirk(void)       { return !!quirkM; }
  bool HasCI(void)          { return hasCiM; }
  void Update(void)         { lastSeenM.Set(); }
  uint64_t LastSeen(void)   { return lastSeenM.Elapsed(); }
  time_t Created(void)      { return createdM; }
};

// --- cSatipServers ----------------------------------------------------------

class cSatipServers : public cList<cSatipServer> {
public:
  cSatipServer *Find(cSatipServer *serverP);
  cSatipServer *Find(int sourceP);
  cSatipServer *Assign(int sourceP, int transponderP, int systemP);
  cSatipServer *Update(cSatipServer *serverP);
  void Use(cSatipServer *serverP, int transponderP, bool onOffP);
  bool IsQuirk(cSatipServer *serverP, int quirkP);
  bool HasCI(cSatipServer *serverP);
  void Cleanup(uint64_t intervalMsP = 0);
  cString GetAddress(cSatipServer *serverP);
  cString GetString(cSatipServer *serverP);
  cString List(void);
  int NumProvidedSystems(void);
};

#endif // __SATIP_SERVER_H
