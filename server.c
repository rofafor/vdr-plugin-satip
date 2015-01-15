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

// --- cSatipServer -----------------------------------------------------------

cSatipServer::cSatipServer(const char *addressP, const char *modelP, const char *descriptionP)
: addressM(addressP),
  modelM(modelP),
  descriptionM(descriptionP),
  modelTypeM(eSatipModelTypeNone),
  quirkM(eSatipQuirkNone),
  useCountM(0),
  transponderM(0),
  createdM(time(NULL)),
  lastSeenM(0)
{
  memset(modelCountM, 0, sizeof(modelCountM));
  if (isempty(*modelM))
     modelM = "DVBS-1";
  if (!isempty(*descriptionM)) {
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
     if (strstr(*descriptionM, "fritzdvbc"))            // Fritz!WLAN Repeater DVB-C
        quirkM |= eSatipQuirkForceLock;
     if (quirkM != eSatipQuirkNone)
        info("Malfunctioning '%s' server detected! Please, fix the firmware.", *descriptionM);
     // These devices support the X_PMT protocol extension
     if (strstr(*descriptionM, "OctopusNet"))           // Digital Devices OctopusNet
        quirkM |= eSatipQuirkUseXCI;
  }
  char *s, *p = strdup(*modelM);
  char *r = strtok_r(p, ",", &s);
  while (r) {
        if (strstr(r, "DVBS2-")) {
           modelTypeM |= eSatipModelTypeDVBS2;
           if (char *c = strstr(r, "-"))
              modelCountM[eSatipModuleDVBS2] = atoi(++c);
           }
        else if (strstr(r, "DVBT2-")) {
           modelTypeM |= eSatipModelTypeDVBT2;
           if (char *c = strstr(r, "-"))
              modelCountM[eSatipModuleDVBT2] = atoi(++c);
           modelTypeM |= eSatipModelTypeDVBT;
           modelCountM[eSatipModuleDVBT] = modelCountM[eSatipModuleDVBT2];
           }
        else if (strstr(r, "DVBT-")) {
           modelTypeM |= eSatipModelTypeDVBT;
           if (char *c = strstr(r, "-"))
              modelCountM[eSatipModuleDVBT] = atoi(++c);
           }
        else if (strstr(r, "DVBC2-")) {
           modelTypeM |= eSatipModelTypeDVBC2;
           if (char *c = strstr(r, "-"))
              modelCountM[eSatipModuleDVBC2] = atoi(++c);
           modelTypeM |= eSatipModelTypeDVBC;
           modelCountM[eSatipModuleDVBC] = modelCountM[eSatipModuleDVBC2];
           }
        else if (strstr(r, "DVBC-")) {
           modelTypeM |= eSatipModelTypeDVBC;
           if (char *c = strstr(r, "-"))
              modelCountM[eSatipModuleDVBC] = atoi(++c);
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

void cSatipServer::Use(bool onOffP)
{
  if (onOffP)
     ++useCountM;
  else
     --useCountM;
}

// --- cSatipServers ----------------------------------------------------------

cSatipServer *cSatipServers::Find(cSatipServer *serverP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP)
         return s;
      }
  return NULL;
}

cSatipServer *cSatipServers::Find(int sourceP, int transponderP, int systemP)
{
  cSatipServer *result = NULL;
  int model = 0;
  if (cSource::IsType(sourceP, 'S'))
     model |= cSatipServer::eSatipModelTypeDVBS2;
  else if (cSource::IsType(sourceP, 'T')) {
     if (systemP < 0)
        model |= cSatipServer::eSatipModelTypeDVBT2 | cSatipServer::eSatipModelTypeDVBT;
     else
        model |= systemP ? cSatipServer::eSatipModelTypeDVBT2 : cSatipServer::eSatipModelTypeDVBT;
     }
  else if (cSource::IsType(sourceP, 'C'))
     model |= cSatipServer::eSatipModelTypeDVBC;
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s->Match(model) && s->Used() && (s->Transponder() == transponderP))
         return s;
      }
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s->Match(model)) {
         result = s;
         if (!s->Used()) {
            break;
            }
         }
      }
  return result;
}

void cSatipServers::SetTransponder(cSatipServer *serverP, int transponderP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         s->SetTransponder(transponderP);
         break;
         }
      }
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

void cSatipServers::Use(cSatipServer *serverP, bool onOffP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         s->Use(onOffP);
         break;
         }
      }
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
      count += s->Satellite() * 4;
      // DVB-T2: qpsk, qam16, qam64, qam256
      // DVB-T: qpsk, qam16, qam64
      count += s->Terrestrial2() ? s->Terrestrial2() * 4 : s->Terrestrial() * 3;
      // DVB-C2: qam16, qam32, qam64, qam128, qam256
      // DVB-C: qam64, qam128, qam256
      count += s->Cable2() ? s->Cable2() * 5 : s->Cable() * 3;
      }
  return count;
}
