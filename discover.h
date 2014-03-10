/*
 * discover.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_DISCOVER_H
#define __SATIP_DISCOVER_H

#include <curl/curl.h>
#include <curl/easy.h>

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "socket.h"

class cSatipServer : public cListObject {
private:
  enum eSatipServer {
    eSatipServerDVBS2 = 0,
    eSatipServerDVBT,
    eSatipServerDVBT2,
    eSatipServerCount
  };
  cString addressM;
  cString descriptionM;
  cString modelM;
  int modelCountM[eSatipServerCount];
  int modelTypeM;
  cTimeMs lastSeenM;

public:
  enum eSatipModelType {
    eSatipModelTypeDVBS2 = 0x01,
    eSatipModelTypeDVBT  = 0x02,
    eSatipModelTypeDVBT2 = 0x04,
    eSatipModelTypeMask  = 0x0F
  };
  cSatipServer(const char *addressP, const char *descriptionP, const char *modelP) : addressM(addressP), descriptionM(descriptionP), modelM(modelP), modelTypeM(eSatipModelTypeMask), lastSeenM(0)
  {
    memset(modelCountM, 0, sizeof(modelCountM));
    if (isempty(*modelM))
       modelM = "DVBS-1,DVBT2-1";
    char *s, *p = strdup(*modelM);
    char *r = strtok_r(p, ",", &s);
    while (r) {
          if (strstr(r, "DVBS2")) {
             modelTypeM |= cSatipServer::eSatipModelTypeDVBS2;
             if (char *c = strstr(r, "-"))
                modelCountM[eSatipServerDVBS2] = atoi(++c);
            }
          if (strstr(r, "DVBT2")) {
             modelTypeM |= cSatipServer::eSatipModelTypeDVBT | cSatipServer::eSatipModelTypeDVBT2;
             if (char *c = strstr(r, "-"))
                modelCountM[eSatipServerDVBT2] = atoi(++c);
            }
          if (strstr(r, "DVBT")) {
             modelTypeM |= cSatipServer::eSatipModelTypeDVBT;
             if (char *c = strstr(r, "-"))
                modelCountM[eSatipServerDVBT] = atoi(++c);
            }
          r = strtok_r(NULL, ",", &s);
          }
    free(p);
  }
  virtual int Compare(const cListObject &listObjectP) const { const cSatipServer *s = (const cSatipServer *)&listObjectP; return strcasecmp(*addressM, *s->addressM); }
  const char *Description() { return *descriptionM; }
  const char *Address() { return *addressM; }
  const char *Model(void) { return modelM; }
  int ModelType(void) { return modelTypeM; }
  bool Match(int modelP) { return ((modelP & eSatipModelTypeMask) & modelTypeM); }
  int Satellite() { return (modelTypeM & eSatipModelTypeDVBS2) ? modelCountM[eSatipServerDVBS2] : 0; }
  int Terrestrial() { return (modelTypeM & eSatipModelTypeDVBT) ? modelCountM[eSatipServerDVBT] : 0; }
  int Terrestrial2() { return (modelTypeM & eSatipModelTypeDVBT2) ? modelCountM[eSatipServerDVBT2] : 0; }
  int LastSeen(void) { return lastSeenM.Elapsed(); }
  void Update(void) { lastSeenM.Set(); }
};

class cSatipServers : public cList<cSatipServer> {
};


class cSatipDiscover : public cThread {
private:
  enum {
    eConnectTimeoutMs = 1500, // in milliseconds
    eDiscoveryPort    = 1900,
    eProbeBufferSize  = 1024, // in bytes
    eProbeTimeoutMs   = 2000, // in milliseconds
    eProbeIntervalMs  = 60000 // in milliseconds
  };
  static cSatipDiscover *instanceS;
  static const char *bcastAddressS;
  static const char *bcastMessageS;
  static size_t WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  cMutex mutexM;
  CURL *handleM;
  cSatipSocket *socketM;
  cCondWait sleepM;
  cTimeMs probeIntervalM;
  cSatipServers *serversM;
  void Janitor(void);
  void Probe(void);
  void Read(void);
  void AddServer(const char *addrP, const char *descP, const char *modelP);
  // constructor
  cSatipDiscover();
  // to prevent copy constructor and assignment
  cSatipDiscover(const cSatipDiscover&);
  cSatipDiscover& operator=(const cSatipDiscover&);

protected:
  virtual void Action(void);

public:
  static cSatipDiscover *GetInstance(void);
  static bool Initialize(void);
  static void Destroy(void);
  virtual ~cSatipDiscover();
  bool IsValidServer(cSatipServer *serverP);
  cSatipServer *GetServer(int sourceP, int systemP = -1);
  cSatipServers *GetServers(void);
  cString GetServerList(void);
  int NumProvidedSystems(void);
};

#endif // __SATIP_DISCOVER_H
