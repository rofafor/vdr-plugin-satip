/*
 * server.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/sources.h>

#include "config.h"
#include "common.h"
#include "log.h"
#include "server.h"

// --- cSatipFrontend ---------------------------------------------------------

cSatipFrontend::cSatipFrontend(const int indexP, const char *descriptionP)
: indexM(indexP),
  transponderM(0),
  descriptionM(descriptionP),
  usedM(false)
{
}

cSatipFrontend::~cSatipFrontend()
{
}

// --- cSatipFrontends --------------------------------------------------------

bool cSatipFrontends::Matches(int transponderP)
{
  for (cSatipFrontend *f = First(); f; f = Next(f)) {
      if (f->IsUsed() && (f->Transponder() == transponderP))
         return true;
      }
  return false;
}

bool cSatipFrontends::Assign(int transponderP)
{
  cSatipFrontend *tmp = First();
  // Prefer any unused one
  for (cSatipFrontend *f = First(); f; f = Next(f)) {
      if (!f->IsUsed()) {
         tmp = f;
         break;
         }
      }
  if (tmp) {
     tmp->SetTransponder(transponderP);
     return true;
     }
  return false;
}

bool cSatipFrontends::Use(int transponderP, bool onOffP)
{
  for (cSatipFrontend *f = First(); f; f = Next(f)) {
      if (f->Transponder() == transponderP) {
         f->Use(onOffP);
         debug9("%s (%d, %d) %s/#%d", __PRETTY_FUNCTION__, transponderP, onOffP, *f->Description(), f->Index());
         return true;
         }
      }
  return false;
}

// --- cSatipServer -----------------------------------------------------------

cSatipServer::cSatipServer(const char *addressP, const char *modelP, const char *descriptionP)
: addressM((addressP && *addressP) ? addressP : "0.0.0.0"),
  modelM((modelP && *modelP) ? modelP : "DVBS-1"),
  descriptionM(!isempty(descriptionP) ? descriptionP : "MyBrokenHardware"),
  quirkM(eSatipQuirkNone),
  hasCiM(false),
  createdM(time(NULL)),
  lastSeenM(0)
{
  if (!SatipConfig.GetDisableServerQuirks()) {
     debug3("%s quirks=%s", __PRETTY_FUNCTION__, *descriptionM);
     // These devices contain a session id bug:
     // Inverto Airscreen Server IDL 400 ?
     // Elgato EyeTV Netstream 4Sat ?
     if (strstr(*descriptionM, "GSSBOX") ||             // Grundig Sat Systems GSS.box DSI 400
         strstr(*descriptionM, "DIGIBIT") ||            // Telestar Digibit R1
         strstr(*descriptionM, "Triax SatIP Converter") // Triax TSS 400
        )
        quirkM |= eSatipQuirkSessionId;
     // These devices contain a play (add/delpids) parameter bug:
     if (strstr(*descriptionM, "fritzdvbc"))            // Fritz!WLAN Repeater DVB-C
        quirkM |= eSatipQuirkPlayPids;
     // These devices contain a frontend locking bug:
     if (strstr(*descriptionM, "fritzdvbc") ||          // Fritz!WLAN Repeater DVB-C
         strstr(*descriptionM, "Triax SatIP Converter") // Triax TSS 400
        )
        quirkM |= eSatipQuirkForceLock;
     }
  // These devices support the X_PMT protocol extension
  if (strstr(*descriptionM, "OctopusNet"))           // Digital Devices OctopusNet
     hasCiM = true;
  char *s, *p = strdup(*modelM);
  char *r = strtok_r(p, ",", &s);
  while (r) {
        char *c;
        if (c = strstr(r, "DVBS2-")) {
           int count = atoi(c + 6);
           for (int i = 1; i <= count; ++i)
               frontendsM[eSatipFrontendDVBS2].Add(new cSatipFrontend(i, "DVB-S2"));
           }
        else if (c = strstr(r, "DVBT-")) {
           int count = atoi(c + 5);
           for (int i = 1; i <= count; ++i)
               frontendsM[eSatipFrontendDVBT].Add(new cSatipFrontend(i, "DVB-T"));
           }
        else if (c = strstr(r, "DVBT2-")) {
           int count = atoi(c + 6);
           for (int i = 1; i <= count; ++i)
               frontendsM[eSatipFrontendDVBT2].Add(new cSatipFrontend(i, "DVB-T2"));
           }
        else if (c = strstr(r, "DVBC-")) {
           int count = atoi(c + 5);
           for (int i = 1; i <= count; ++i)
               frontendsM[eSatipFrontendDVBC].Add(new cSatipFrontend(i, "DVB-C"));
           }
        else if (c = strstr(r, "DVBC2-")) {
           int count = atoi(c + 6);
           for (int i = 1; i <= count; ++i)
               frontendsM[eSatipFrontendDVBC2].Add(new cSatipFrontend(i, "DVB-C2"));
           }
        r = strtok_r(NULL, ",", &s);
        }
  free(p);
}

cSatipServer::~cSatipServer()
{
}

int cSatipServer::Compare(const cListObject &listObjectP) const
{
  const cSatipServer *s = (const cSatipServer *)&listObjectP;
  int result = strcasecmp(*addressM, *s->addressM);
  if (!result) {
     result = strcasecmp(*modelM, *s->modelM);
     if (!result)
        result = strcasecmp(*descriptionM, *s->descriptionM);
     }
  return result;
}

bool cSatipServer::Assign(int sourceP, int systemP, int transponderP)
{
  bool result = false;
  if (cSource::IsType(sourceP, 'S'))
     result = frontendsM[eSatipFrontendDVBS2].Assign(transponderP);
  else if (cSource::IsType(sourceP, 'T')) {
     if (systemP)
        result = frontendsM[eSatipFrontendDVBT2].Assign(transponderP);
     else
        result = frontendsM[eSatipFrontendDVBT].Assign(transponderP) || frontendsM[eSatipFrontendDVBT2].Assign(transponderP);
     }
  else if (cSource::IsType(sourceP, 'C')) {
     if (systemP)
        result = frontendsM[eSatipFrontendDVBC2].Assign(transponderP);
     else
        result = frontendsM[eSatipFrontendDVBC].Assign(transponderP) || frontendsM[eSatipFrontendDVBC2].Assign(transponderP);
     }
  return result;
}

bool cSatipServer::Matches(int sourceP)
{
  if (cSource::IsType(sourceP, 'S'))
     return GetModulesDVBS2();
  else if (cSource::IsType(sourceP, 'T'))
     return GetModulesDVBT() || GetModulesDVBT2();
  else if (cSource::IsType(sourceP, 'C'))
     return GetModulesDVBC() || GetModulesDVBC2();
  return false;
}

bool cSatipServer::Matches(int sourceP, int systemP, int transponderP)
{
  bool result = false;
  if (cSource::IsType(sourceP, 'S'))
     result = frontendsM[eSatipFrontendDVBS2].Matches(transponderP);
  else if (cSource::IsType(sourceP, 'T')) {
     if (systemP)
        result = frontendsM[eSatipFrontendDVBT2].Matches(transponderP);
     else
        result = frontendsM[eSatipFrontendDVBT].Matches(transponderP) || frontendsM[eSatipFrontendDVBT2].Matches(transponderP);
     }
  else if (cSource::IsType(sourceP, 'C')) {
     if (systemP)
        result = frontendsM[eSatipFrontendDVBC2].Matches(transponderP);
     else
        result = frontendsM[eSatipFrontendDVBC].Matches(transponderP) || frontendsM[eSatipFrontendDVBC2].Matches(transponderP);
     }
  return result;
}

void cSatipServer::Use(int transponderP, bool onOffP)
{
  for (int i = 0; i < eSatipFrontendCount; ++i) {
      if (frontendsM[i].Use(transponderP, onOffP))
         return;
      }
}

int cSatipServer::GetModulesDVBS2(void)
{
  return frontendsM[eSatipFrontendDVBS2].Count();
}

int cSatipServer::GetModulesDVBT(void)
{
  return frontendsM[eSatipFrontendDVBT].Count();
}

int cSatipServer::GetModulesDVBT2(void)
{
  return frontendsM[eSatipFrontendDVBT2].Count();
}

int cSatipServer::GetModulesDVBC(void)
{
  return frontendsM[eSatipFrontendDVBC].Count();
}

int cSatipServer::GetModulesDVBC2(void)
{
  return frontendsM[eSatipFrontendDVBC2].Count();
}

// --- cSatipServers ----------------------------------------------------------

cSatipServer *cSatipServers::Find(cSatipServer *serverP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s->Compare(*serverP) == 0)
         return s;
      }
  return NULL;
}

cSatipServer *cSatipServers::Find(int sourceP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s->Matches(sourceP))
         return s;
      }
  return NULL;
}

cSatipServer *cSatipServers::Assign(int sourceP, int transponderP, int systemP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s->Matches(sourceP, systemP, transponderP))
         return s;
      }
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s->Assign(sourceP, systemP, transponderP))
         return s;
      }
  return NULL;
}

cSatipServer *cSatipServers::Update(cSatipServer *serverP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s->Compare(*serverP) == 0) {
         s->Update();
         return s;
         }
      }
  return NULL;
}

void cSatipServers::Use(cSatipServer *serverP, int transponderP, bool onOffP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         s->Use(transponderP, onOffP);
         break;
         }
      }
}

bool cSatipServers::IsQuirk(cSatipServer *serverP, int quirkP)
{
  bool result = false;
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         result = s->Quirk(quirkP);
         break;
         }
      }
  return result;
}

bool cSatipServers::HasCI(cSatipServer *serverP)
{
  bool result = false;
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         result = s->HasCI();
         break;
         }
      }
  return result;
}

void cSatipServers::Cleanup(uint64_t intervalMsP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (!intervalMsP || (s->LastSeen() > intervalMsP)) {
         info("Removing server %s (%s %s)", s->Description(), s->Address(), s->Model());
         Del(s);
         }
      }
}

cString cSatipServers::GetAddress(cSatipServer *serverP)
{
  cString address = "";
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         address = s->Address();
         break;
         }
      }
  return address;
}

cString cSatipServers::GetString(cSatipServer *serverP)
{
  cString list = "";
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         list = cString::sprintf("%s|%s|%s", s->Address(), s->Model(), s->Description());
         break;
         }
      }
  return list;
}

cString cSatipServers::List(void)
{
  cString list = "";
  for (cSatipServer *s = First(); s; s = Next(s))
      list = cString::sprintf("%s%s|%s|%s\n", *list, s->Address(), s->Model(), s->Description());
  return list;
}

int cSatipServers::NumProvidedSystems(void)
{
  int count = 0;
  for (cSatipServer *s = First(); s; s = Next(s)) {
      // DVB-S2: qpsk, 8psk, 16apsk, 32apsk
      count += s->GetModulesDVBS2() * 4;
      // DVB-T: qpsk, qam16, qam64
      count += s->GetModulesDVBT() * 3;
      // DVB-T2: qpsk, qam16, qam64, qam256
      count += s->GetModulesDVBT2() * 4;
      // DVB-C: qam64, qam128, qam256
      count += s->GetModulesDVBC() * 3;
      // DVB-C2: qam16, qam32, qam64, qam128, qam256
      count += s->GetModulesDVBC2() * 5;
      }
  return count;
}
