/*
 * discover.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>

#include "common.h"
#include "config.h"
#include "socket.h"
#include "discover.h"

cSatipDiscover *cSatipDiscover::instanceS = NULL;

const char *cSatipDiscover::bcastAddressS = "239.255.255.250";
const char *cSatipDiscover::bcastMessageS = "M-SEARCH * HTTP/1.1\r\n"                  \
                                            "HOST: 239.255.255.250:1900\r\n"           \
                                            "MAN: \"ssdp:discover\"\r\n"               \
                                            "ST: urn:ses-com:device:SatIPServer:1\r\n" \
                                            "MX: 2\r\n\r\n";

cSatipDiscover *cSatipDiscover::GetInstance(void)
{
  if (!instanceS)
     instanceS = new cSatipDiscover();
  return instanceS;
}

bool cSatipDiscover::Initialize(void)
{
  debug("cSatipDiscover::%s()", __FUNCTION__);
  return true;
}

void cSatipDiscover::Destroy(void)
{
  debug("cSatipDiscover::%s()", __FUNCTION__);
  DELETE_POINTER(instanceS);
}

size_t cSatipDiscover::WriteCallback(void *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipDiscover *obj = reinterpret_cast<cSatipDiscover *>(dataP);
  size_t len = sizeP * nmembP;
  //debug("cSatipDiscover::%s(%zu)", __FUNCTION__, len);

  char *s, *p = (char *)ptrP;
  char *r = strtok_r(p, "\r\n", &s);
  char *desc = NULL, *model = NULL, *addr = NULL;
  while (r) {
    //debug("cSatipDiscover::%s(%zu): %s", __FUNCTION__, len, r);
    // <friendlyName>OctopusNet</friendlyName>
    if (startswith(r, "<friendlyName"))
       desc = StripTags(r);
    // <satip:X_SATIPCAP xmlns:satip="urn:ses-com:satip">DVBT-2</satip:X_SATIPCAP>
    if (startswith(r, "<satip:X_SATIPCAP"))
       model = StripTags(r);
    r = strtok_r(NULL, "\r\n", &s);
    }
  if (obj) {
     CURLcode res = CURLE_OK;
     SATIP_CURL_EASY_GETINFO(obj->handleM, CURLINFO_PRIMARY_IP, &addr);
     obj->AddServer(addr, desc, model);
     }

  return len;
}

cSatipDiscover::cSatipDiscover()
: cThread("SAT>IP discover"),
  mutexM(),
  handleM(curl_easy_init()),
  socketM(new cSatipSocket()),
  sleepM(),
  probeIntervalM(0),
  serversM(new cSatipServers())
{
  debug("cSatipDiscover::%s()", __FUNCTION__);
  // Start the thread
  Start();
}

cSatipDiscover::~cSatipDiscover()
{
  debug("cSatipDiscover::%s()", __FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  sleepM.Signal();
  if (Running())
     Cancel(3);
  // Free allocated memory
  DELETENULL(socketM);
  DELETENULL(serversM);
  if (handleM)
     curl_easy_cleanup(handleM);
  handleM = NULL;
}

void cSatipDiscover::Action(void)
{
  debug("cSatipDiscover::%s(): entering", __FUNCTION__);
  // Do the thread loop
  while (Running()) {
        if (probeIntervalM.TimedOut()) {
           probeIntervalM.Set(eProbeIntervalMs);
           Probe();
           Janitor();
           }
        // to avoid busy loop and reduce cpu load
        sleepM.Wait(10);
        }
  debug("cSatipDiscover::%s(): exiting", __FUNCTION__);
}

void cSatipDiscover::Janitor(void)
{
  debug("cSatipDiscover::%s()", __FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  for (cSatipServer *srv = serversM->First(); srv; srv = serversM->Next(srv)) {
      if (srv->LastSeen() > eProbeIntervalMs * 2) {
         info("Removing device %s (%s %s)", srv->Description(), srv->Address(), srv->Model());
         serversM->Del(srv);
         }
      }
}

void cSatipDiscover::Probe(void)
{
  debug("cSatipDiscover::%s()", __FUNCTION__);
  if (socketM && socketM->Open(eDiscoveryPort)) {
     cTimeMs timeout(eProbeTimeoutMs);
     socketM->Write(bcastAddressS, reinterpret_cast<const unsigned char *>(bcastMessageS), strlen(bcastMessageS));
     while (Running() && !timeout.TimedOut()) {
           Read();
           // to avoid busy loop and reduce cpu load
           sleepM.Wait(100);
           }
     socketM->Close();
     }
}

void cSatipDiscover::Read(void)
{
  //debug("cSatipDiscover::%s()", __FUNCTION__);
  if (socketM) {
     unsigned char *buf = MALLOC(unsigned char, eProbeBufferSize + 1);
     if (buf) {
        memset(buf, 0, eProbeBufferSize + 1);
        int len = socketM->Read(buf, eProbeBufferSize);
        if (len > 0) {
           //debug("cSatipDiscover::%s(): len=%d", __FUNCTION__, len);
           bool status = false;
           char *s, *p = reinterpret_cast<char *>(buf), *location = NULL;
           char *r = strtok_r(p, "\r\n", &s);
           while (r) {
                 //debug("cSatipDiscover::%s(): %s", __FUNCTION__, r);
                 // Check the status code
                 // HTTP/1.1 200 OK
                 if (!status && startswith(r, "HTTP/1.1 200 OK")) {
                     status = true;
                     }
                 // Check the location data
                 // LOCATION: http://192.168.0.115:8888/octonet.xml
                 if (status && startswith(r, "LOCATION:")) {
                     location = compactspace(r + 9);
                     debug("cSatipDiscover::%s(): location='%s'", __FUNCTION__, location);
                     break;
                     }
                 r = strtok_r(NULL, "\r\n", &s);
                 }
           if (handleM && !isempty(location)) {
              long rc = 0;
              CURLcode res = CURLE_OK;
#ifdef DEBUG
              // Verbose output
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_VERBOSE, 1L);
#endif
              // Set callback
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEFUNCTION, cSatipDiscover::WriteCallback);
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_WRITEDATA, this);

              // No progress meter and no signaling
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOPROGRESS, 1L);
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_NOSIGNAL, 1L);

              // Set timeouts
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_TIMEOUT_MS, (long)eConnectTimeoutMs);
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_CONNECTTIMEOUT_MS, (long)eConnectTimeoutMs);

              // Set user-agent
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_USERAGENT, *cString::sprintf("vdr-%s/%s", PLUGIN_NAME_I18N, VERSION));

              // Set URL
              SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_URL, location);

              // Fetch the data
              SATIP_CURL_EASY_PERFORM(handleM);
              SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_RESPONSE_CODE, &rc);
              if (rc != 200)
                 error("Discovery detected invalid status code: %ld", rc);
              }
           }
        free(buf);
        }
     }
}

void cSatipDiscover::AddServer(const char *addrP, const char *descP, const char * modelP)
{
  debug("cSatipDiscover::%s(%s, %s, %s)", __FUNCTION__, addrP, descP, modelP);
  cMutexLock MutexLock(&mutexM);
  if (serversM) {
     cSatipServer *tmp = new cSatipServer(addrP, descP, modelP);
     // Validate against existing servers
     bool found = false;
     for (cSatipServer *s = serversM->First(); s; s = serversM->Next(s)) {
         if (s->Compare(*tmp) == 0) {
            found = true;
            s->Update();
            break;
            }
         }
     if (!found) {
        info("Adding device %s (%s %s)", tmp->Description(), tmp->Address(), tmp->Model());
        serversM->Add(tmp);
        }
     else
        DELETENULL(tmp);
     }
}

bool cSatipDiscover::IsValidServer(cSatipServer *serverP)
{
  //debug("cSatipDiscover::%s(%d)", __FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  for (cSatipServer *srv = serversM->First(); srv; srv = serversM->Next(srv)) {
      if (srv == serverP)
         return true;
      }
  return false;
}

cSatipServer *cSatipDiscover::GetServer(int modelP)
{
  //debug("cSatipDiscover::%s(%d)", __FUNCTION__, modelP);
  cMutexLock MutexLock(&mutexM);
  for (cSatipServer *srv = serversM->First(); srv; srv = serversM->Next(srv)) {
      if (srv->Match(modelP))
         return srv;
      }
  return NULL;
}

cSatipServers *cSatipDiscover::GetServers(void)
{
  //debug("cSatipDiscover::%s()", __FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  return serversM;
}

cString cSatipDiscover::GetServerList(void)
{
  //debug("cSatipDiscover::%s(%d)", __FUNCTION__, modelP);
  cMutexLock MutexLock(&mutexM);
  cString list = "";
  for (cSatipServer *srv = serversM->First(); srv; srv = serversM->Next(srv))
      list = cString::sprintf("%s%s:%s:%s\n", *list, srv->Address(), srv->Model(), srv->Description());
  return list;
}

int cSatipDiscover::NumProvidedSystems(void)
{
  //debug("cSatipDiscover::%s(%d)", __FUNCTION__, modelP);
  cMutexLock MutexLock(&mutexM);
  int count = 0;
  for (cSatipServer *srv = serversM->First(); srv; srv = serversM->Next(srv)) {
      count += srv->Satellite();
      count += srv->Terrestrial();
      count += srv->Terrestrial2();
      }
  return count;
}
