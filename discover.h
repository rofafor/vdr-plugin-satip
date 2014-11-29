/*
 * discover.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_DISCOVER_H
#define __SATIP_DISCOVER_H

#include <curl/curl.h>

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "discoverif.h"
#include "msearch.h"
#include "server.h"
#include "socket.h"

class cSatipDiscoverServer : public cListObject {
private:
  cString ipAddressM;
  cString descriptionM;
  cString modelM;
public:
  cSatipDiscoverServer(const char *ipAddressP, const char *modelP, const char *descriptionP)
  {
    ipAddressM = ipAddressP; modelM = modelP; descriptionM = descriptionP;
  }
  const char *IpAddress(void)   { return *ipAddressM; }
  const char *Model(void)       { return *modelM; }
  const char *Description(void) { return *descriptionM; }
};

class cSatipDiscoverServers : public cList<cSatipDiscoverServer> {
};

class cSatipDiscover : public cThread, public cSatipDiscoverIf {
private:
  enum {
    eSleepTimeoutMs   = 500,  // in milliseconds
    eConnectTimeoutMs = 1500, // in milliseconds
    eProbeTimeoutMs   = 2000, // in milliseconds
    eProbeIntervalMs  = 60000 // in milliseconds
  };
  static cSatipDiscover *instanceS;
  static size_t WriteCallback(char *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  static int    DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP);
  cMutex mutexM;
  cSatipMsearch msearchM;
  cStringList probeUrlListM;
  CURL *handleM;
  cCondWait sleepM;
  cTimeMs probeIntervalM;
  cSatipServers serversM;
  void Activate(void);
  void Deactivate(void);
  void AddServer(const char *addrP, const char *modelP, const char *descP);
  void Fetch(const char *urlP);
  // constructor
  cSatipDiscover();
  // to prevent copy constructor and assignment
  cSatipDiscover(const cSatipDiscover&);
  cSatipDiscover& operator=(const cSatipDiscover&);

protected:
  virtual void Action(void);

public:
  static cSatipDiscover *GetInstance(void);
  static bool Initialize(cSatipDiscoverServers *serversP);
  static void Destroy(void);
  virtual ~cSatipDiscover();
  void TriggerScan(void) { probeIntervalM.Set(0); }
  int GetServerCount(void);
  cSatipServer *GetServer(int sourceP, int transponderP = 0, int systemP = -1);
  cSatipServer *GetServer(cSatipServer *serverP);
  cSatipServers *GetServers(void);
  cString GetServerString(cSatipServer *serverP);
  void SetTransponder(cSatipServer *serverP, int transponderP);
  void UseServer(cSatipServer *serverP, bool onOffP);
  cString GetServerList(void);
  int NumProvidedSystems(void);

  // for internal discover interface
public:
  virtual void SetUrl(const char *urlP);
};

#endif // __SATIP_DISCOVER_H
