/*
 * server.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/sources.h>

#include "common.h"
#include "server.h"

// --- cSatipServer -----------------------------------------------------------

cSatipServer::cSatipServer(const char *addressP, const char *descriptionP, const char *modelP)
: addressM(addressP),
  descriptionM(descriptionP),
  modelM(modelP),
  modelTypeM(eSatipModelTypeMask),
  useCountM(0),
  createdM(time(NULL)),
  lastSeenM(0)
{
  memset(modelCountM, 0, sizeof(modelCountM));
  if (isempty(*modelM))
     modelM = "DVBS-1";
  char *s, *p = strdup(*modelM);
  char *r = strtok_r(p, ",", &s);
  while (r) {
        if (strstr(r, "DVBS2")) {
           modelTypeM |= cSatipServer::eSatipModelTypeDVBS2;
           if (char *c = strstr(r, "-"))
              modelCountM[eSatipModuleDVBS2] = atoi(++c);
           else
              modelCountM[eSatipModuleDVBS2] = 1;
           }
        if (strstr(r, "DVBT2")) {
           modelTypeM |= cSatipServer::eSatipModelTypeDVBT | cSatipServer::eSatipModelTypeDVBT2;
           if (char *c = strstr(r, "-"))
              modelCountM[eSatipModuleDVBT2] = atoi(++c);
           else
              modelCountM[eSatipModuleDVBT2] = 1;
           // Add model quirks here
           if (strstr(*addressM, "OctopusNet"))
              modelTypeM |= cSatipServer::eSatipModelTypeDVBC;
           }
        if (strstr(r, "DVBT")) {
           modelTypeM |= cSatipServer::eSatipModelTypeDVBT;
           if (char *c = strstr(r, "-"))
              modelCountM[eSatipModuleDVBT] = atoi(++c);
           else
              modelCountM[eSatipModuleDVBT] = 1;
           // Add model quirks here
           if (strstr(*addressM, "OctopusNet"))
              modelTypeM |= cSatipServer::eSatipModelTypeDVBC;
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
  return strcasecmp(*addressM, *s->addressM);
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

cSatipServer *cSatipServers::Find(int sourceP, int systemP)
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
      if (s->Match(model)) {
         result = s;
         if (!s->Used()) {
            break;
            }
         }
      }
  return result;
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
         info("Removing device %s (%s %s)", s->Description(), s->Address(), s->Model());
         Del(s);
         }
      }
}

cString cSatipServers::GetString(cSatipServer *serverP)
{
  cString list = "";
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         list = cString::sprintf("%s:%s:%s", s->Address(), s->Model(), s->Description());
         break;
         }
      }
  return list;
}

cString cSatipServers::List(void)
{
  cString list = "";
  for (cSatipServer *s = First(); s; s = Next(s))
      list = cString::sprintf("%s%s:%s:%s\n", *list, s->Address(), s->Model(), s->Description());
  return list;
}

int cSatipServers::NumProvidedSystems(void)
{
  int count = 0;
  for (cSatipServer *s = First(); s; s = Next(s)) {
      // DVB-S*: qpsk, 8psk
      count += s->Satellite() * 4;
      // DVB-T*: qpsk, qam16, qam64, qam256
      count += (s->Terrestrial2() ? s->Terrestrial2() : s->Terrestrial()) * 4;
      }
  return count;
}
