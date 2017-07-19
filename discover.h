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

#include "common.h"
#include "discoverif.h"
#include "msearch.h"
#include "server.h"
#include "socket.h"

class cSatipDiscoverServer : public cListObject {
private:
  int ipPortM;
  int quirkM;
  cString srcAddressM;
  cString ipAddressM;
  cString descriptionM;
  cString modelM;
  cString filtersM;
public:
  cSatipDiscoverServer(const char *srcAddressP, const char *ipAddressP, const int ipPortP, const char *modelP, const char *filtersP, const char *descriptionP, const int quirkP)
  {
     srcAddressM = srcAddressP; ipAddressM = ipAddressP; ipPortM = ipPortP; modelM = modelP; filtersM = filtersP; descriptionM = descriptionP; quirkM = quirkP;
  }
  int IpPort(void)              { return ipPortM; }
  int Quirk(void)               { return quirkM; }
  const char *SrcAddress(void)  { return *srcAddressM; }
  const char *IpAddress(void)   { return *ipAddressM; }
  const char *Model(void)       { return *modelM; }
  const char *Filters(void)     { return *filtersM; }
  const char *Description(void) { return *descriptionM; }
};

class cSatipDiscoverServers : public cList<cSatipDiscoverServer> {
};

class cSatipDiscover : public cThread, public cSatipDiscoverIf {
private:
  enum {
    eSleepTimeoutMs   = 500,   // in milliseconds
    eConnectTimeoutMs = 1500,  // in milliseconds
    eProbeTimeoutMs   = 2000,  // in milliseconds
    eProbeIntervalMs  = 60000, // in milliseconds
    eCleanupTimeoutMs = 124000 // in milliseoonds
  };
  static cSatipDiscover *instanceS;
  static size_t HeaderCallback(char *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  static size_t DataCallback(char *ptrP, size_t sizeP, size_t nmembP, void *dataP);
  static int    DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP);
  cMutex mutexM;
  cSatipMemoryBuffer headerBufferM;
  cSatipMemoryBuffer dataBufferM;
  cSatipMsearch msearchM;
  cStringList probeUrlListM;
  CURL *handleM;
  cCondWait sleepM;
  cTimeMs probeIntervalM;
  cSatipServers serversM;
  void Activate(void);
  void Deactivate(void);
  int ParseRtspPort(void);
  void ParseDeviceInfo(const char *addrP, const int portP);
  void AddServer(const char *srcAddrP, const char *addrP, const int portP, const char *modelP, const char *filtersP, const char *descP, const int quirkP);
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
  cSatipServer *AssignServer(int deviceIdP, int sourceP, int transponderP, int systemP);
  cSatipServer *GetServer(int sourceP);
  cSatipServer *GetServer(cSatipServer *serverP);
  cSatipServers *GetServers(void);
  cString GetServerString(cSatipServer *serverP);
  void ActivateServer(cSatipServer *serverP, bool onOffP);
  void AttachServer(cSatipServer *serverP, int deviceIdP, int transponderP);
  void DetachServer(cSatipServer *serverP, int deviceIdP, int transponderP);
  bool IsServerQuirk(cSatipServer *serverP, int quirkP);
  bool HasServerCI(cSatipServer *serverP);
  cString GetServerAddress(cSatipServer *serverP);
  cString GetSourceAddress(cSatipServer *serverP);
  int GetServerPort(cSatipServer *serverP);
  cString GetServerList(void);
  int NumProvidedSystems(void);

  // for internal discover interface
public:
  virtual void SetUrl(const char *urlP);
};

#endif // __SATIP_DISCOVER_H
