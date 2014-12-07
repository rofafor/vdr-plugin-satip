/*
 * discover.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string.h>
#ifdef USE_TINYXML
 #include <tinyxml.h>
#else
 #include <pugixml.hpp>
#endif
#include "common.h"
#include "config.h"
#include "log.h"
#include "socket.h"
#include "discover.h"

cSatipDiscover *cSatipDiscover::instanceS = NULL;

cSatipDiscover *cSatipDiscover::GetInstance(void)
{
  if (!instanceS)
     instanceS = new cSatipDiscover();
  return instanceS;
}

bool cSatipDiscover::Initialize(cSatipDiscoverServers *serversP)
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (instanceS) {
       if (serversP) {
          for (cSatipDiscoverServer *s = serversP->First(); s; s = serversP->Next(s))
              instanceS->AddServer(s->IpAddress(), s->Model(), s->Description());
          }
     else
        instanceS->Activate();
     }
  return true;
}

void cSatipDiscover::Destroy(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (instanceS)
     instanceS->Deactivate();
}

size_t cSatipDiscover::WriteCallback(char *ptrP, size_t sizeP, size_t nmembP, void *dataP)
{
  cSatipDiscover *obj = reinterpret_cast<cSatipDiscover *>(dataP);
  size_t len = sizeP * nmembP;
  debug8("%s len=%zu", __PRETTY_FUNCTION__, len);

  if (obj) {
     CURLcode res = CURLE_OK;
     const char *desc = NULL, *model = NULL, *addr = NULL;
#ifdef USE_TINYXML
     TiXmlDocument doc;
     char *xml = MALLOC(char, len + 1);
     memcpy(xml, ptrP, len);
     *(xml + len + 1) = 0;
     doc.Parse((const char *)xml);
     TiXmlHandle docHandle(&doc);
     TiXmlElement *descElement = docHandle.FirstChild("root").FirstChild("device").FirstChild("friendlyName").ToElement();
     if (descElement)
        desc = descElement->GetText() ? descElement->GetText() : "MyBrokenHardware";
     TiXmlElement *modelElement = docHandle.FirstChild("root").FirstChild("device").FirstChild("satip:X_SATIPCAP").ToElement();
     if (modelElement)
        model = modelElement->GetText() ? modelElement->GetText() : "DVBS2-1";
#else
     pugi::xml_document doc;
     pugi::xml_parse_result result = doc.load_buffer(ptrP, len);
     if (result) {
        pugi::xml_node descNode = doc.first_element_by_path("root/device/friendlyName");
        if (descNode)
           desc = descNode.text().as_string("MyBrokenHardware");
        pugi::xml_node modelNode = doc.first_element_by_path("root/device/satip:X_SATIPCAP");
        if (modelNode)
           model = modelNode.text().as_string("DVBS2-1");
        }
#endif
     SATIP_CURL_EASY_GETINFO(obj->handleM, CURLINFO_PRIMARY_IP, &addr);
     obj->AddServer(addr, model, desc);
     }

  return len;
}

int cSatipDiscover::DebugCallback(CURL *handleP, curl_infotype typeP, char *dataP, size_t sizeP, void *userPtrP)
{
  cSatipDiscover *obj = reinterpret_cast<cSatipDiscover *>(userPtrP);

  if (obj) {
     switch (typeP) {
       case CURLINFO_TEXT:
            debug2("%s HTTP INFO %.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_IN:
            debug2("%s HTTP HEAD <<< %.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       case CURLINFO_HEADER_OUT:
            debug2("%s HTTP HEAD >>>\n%.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       case CURLINFO_DATA_IN:
            debug2("%s HTTP DATA <<< %.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       case CURLINFO_DATA_OUT:
            debug2("%s HTTP DATA >>>\n%.*s", __PRETTY_FUNCTION__, (int)sizeP, dataP);
            break;
       default:
            break;
       }
     }

  return 0;
}

cSatipDiscover::cSatipDiscover()
: cThread("SATIP discover"),
  mutexM(),
  msearchM(*this),
  probeUrlListM(),
  handleM(curl_easy_init()),
  sleepM(),
  probeIntervalM(0),
  serversM()
{
  debug1("%s", __PRETTY_FUNCTION__);
}

cSatipDiscover::~cSatipDiscover()
{
  debug1("%s", __PRETTY_FUNCTION__);
  Deactivate();
  cMutexLock MutexLock(&mutexM);
  // Free allocated memory
  if (handleM)
     curl_easy_cleanup(handleM);
  handleM = NULL;
  probeUrlListM.Clear();
}

void cSatipDiscover::Activate(void)
{
  // Start the thread
  Start();
}

void cSatipDiscover::Deactivate(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  sleepM.Signal();
  if (Running())
     Cancel(3);
}

void cSatipDiscover::Action(void)
{
  debug1("%s Entering", __PRETTY_FUNCTION__);
  probeIntervalM.Set(eProbeIntervalMs);
  msearchM.Probe();
  // Do the thread loop
  while (Running()) {
        cStringList tmp;

        if (probeIntervalM.TimedOut()) {
           probeIntervalM.Set(eProbeIntervalMs);
           msearchM.Probe();
           mutexM.Lock();
           serversM.Cleanup(eProbeIntervalMs * 2);
           mutexM.Unlock();
           }
        mutexM.Lock();
        if (probeUrlListM.Size()) {
           for (int i = 0; i < probeUrlListM.Size(); ++i)
               tmp.Insert(strdup(probeUrlListM.At(i)));
           probeUrlListM.Clear();
           }
        mutexM.Unlock();
        if (tmp.Size()) {
           for (int i = 0; i < tmp.Size(); ++i)
               Fetch(tmp.At(i));
           tmp.Clear();
           }
        // to avoid busy loop and reduce cpu load
        sleepM.Wait(eSleepTimeoutMs);
        }
  debug1("%s Exiting", __PRETTY_FUNCTION__);
}

void cSatipDiscover::Fetch(const char *urlP)
{
  debug1("%s (%s)", __PRETTY_FUNCTION__, urlP);
  if (handleM && !isempty(urlP)) {
     long rc = 0;
     CURLcode res = CURLE_OK;

     // Verbose output
     if (SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug2)) {
        SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_VERBOSE, 1L);
        SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_DEBUGFUNCTION, cSatipDiscover::DebugCallback);
        SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_DEBUGDATA, this);
        }

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
     SATIP_CURL_EASY_SETOPT(handleM, CURLOPT_URL, urlP);

     // Fetch the data
     SATIP_CURL_EASY_PERFORM(handleM);
     SATIP_CURL_EASY_GETINFO(handleM, CURLINFO_RESPONSE_CODE, &rc);
     if (rc != 200)
        error("Discovery detected invalid status code: %ld", rc);
     }
}

void cSatipDiscover::AddServer(const char *addrP, const char *modelP, const char * descP)
{
  debug1("%s (%s, %s, %s)", __PRETTY_FUNCTION__, addrP, modelP, descP);
  cMutexLock MutexLock(&mutexM);
  cSatipServer *tmp = new cSatipServer(addrP, modelP, descP);
  // Validate against existing servers
  if (!serversM.Update(tmp)) {
     info("Adding device '%s|%s|%s'",  tmp->Address(), tmp->Model(), tmp->Description());
     serversM.Add(tmp);
     }
  else
     DELETENULL(tmp);
}

int cSatipDiscover::GetServerCount(void)
{
  debug8("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  return serversM.Count();
}

cSatipServer *cSatipDiscover::GetServer(int sourceP, int transponderP, int systemP)
{
  debug8("%s (%d, %d, %d)", __PRETTY_FUNCTION__, sourceP, transponderP, systemP);
  cMutexLock MutexLock(&mutexM);
  return serversM.Find(sourceP, transponderP, systemP);
}

cSatipServer *cSatipDiscover::GetServer(cSatipServer *serverP)
{
  debug8("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  return serversM.Find(serverP);
}

cSatipServers *cSatipDiscover::GetServers(void)
{
  debug8("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  return &serversM;
}

cString cSatipDiscover::GetServerString(cSatipServer *serverP)
{
  debug8("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  return serversM.GetString(serverP);
}

cString cSatipDiscover::GetServerList(void)
{
  debug8("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  return serversM.List();
}

void cSatipDiscover::SetTransponder(cSatipServer *serverP, int transponderP)
{
  debug8("%s (, %d)", __PRETTY_FUNCTION__, transponderP);
  cMutexLock MutexLock(&mutexM);
  serversM.SetTransponder(serverP, transponderP);
}

void cSatipDiscover::UseServer(cSatipServer *serverP, bool onOffP)
{
  debug8("%s (, %d)", __PRETTY_FUNCTION__, onOffP);
  cMutexLock MutexLock(&mutexM);
  serversM.Use(serverP, onOffP);
}

int cSatipDiscover::NumProvidedSystems(void)
{
  debug8("%s", __PRETTY_FUNCTION__);
  cMutexLock MutexLock(&mutexM);
  return serversM.NumProvidedSystems();
}

void cSatipDiscover::SetUrl(const char *urlP)
{
  debug8("%s (%s)", __PRETTY_FUNCTION__, urlP);
  mutexM.Lock();
  probeUrlListM.Insert(strdup(urlP));
  mutexM.Unlock();
  sleepM.Signal();
}
