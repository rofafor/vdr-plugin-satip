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
  deviceIdM(-1),
  descriptionM(descriptionP)
{
}

cSatipFrontend::~cSatipFrontend()
{
}

// --- cSatipFrontends --------------------------------------------------------

bool cSatipFrontends::Matches(int deviceIdP, int transponderP)
{
  for (cSatipFrontend *f = First(); f; f = Next(f)) {
      if (f->Attached() && (f->DeviceId() == deviceIdP) && (f->Transponder() == transponderP))
         return true;
      }
  return false;
}

bool cSatipFrontends::Assign(int deviceIdP, int transponderP)
{
  cSatipFrontend *tmp = NULL;
  // Prefer any unused one
  for (cSatipFrontend *f = First(); f; f = Next(f)) {
      if (!f->Attached() || (f->DeviceId() == deviceIdP)) {
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

bool cSatipFrontends::Attach(int deviceIdP, int transponderP)
{
  for (cSatipFrontend *f = First(); f; f = Next(f)) {
      if (f->Transponder() == transponderP) {
         f->Attach(deviceIdP);
         debug9("%s (%d, %d) %s/#%d", __PRETTY_FUNCTION__, deviceIdP, transponderP, *f->Description(), f->Index());
         return true;
         }
      }
  return false;
}

bool cSatipFrontends::Detach(int deviceIdP, int transponderP)
{
  for (cSatipFrontend *f = First(); f; f = Next(f)) {
      if (f->Transponder() == transponderP) {
         f->Detach(deviceIdP);
         debug9("%s (%d, %d) %s/#%d", __PRETTY_FUNCTION__, deviceIdP, transponderP, *f->Description(), f->Index());
         return true;
         }
      }
  return false;
}

// --- cSatipServer -----------------------------------------------------------

cSatipServer::cSatipServer(const char *srcAddressP, const char *addressP, const int portP, const char *modelP, const char *filtersP, const char *descriptionP, const int quirkP)
: srcAddressM((srcAddressP && *srcAddressP) ? srcAddressP : ""),
  addressM((addressP && *addressP) ? addressP : "0.0.0.0"),
  modelM((modelP && *modelP) ? modelP : "DVBS-1"),
  filtersM((filtersP && *filtersP) ? filtersP : ""),
  descriptionM(!isempty(descriptionP) ? descriptionP : "MyBrokenHardware"),
  quirksM(""),
  portM(portP),
  quirkM(quirkP),
  hasCiM(false),
  activeM(true),
  createdM(time(NULL)),
  lastSeenM(0)
{
  memset(sourceFiltersM, 0, sizeof(sourceFiltersM));
  if (!isempty(*filtersM)) {
     char *s, *p = strdup(*filtersM);
     char *r = strtok_r(p, ",", &s);
     unsigned int i = 0;
     while (r) {
           int t = cSource::FromString(skipspace(r));
           if (t && i < ELEMENTS(sourceFiltersM))
              sourceFiltersM[i++] = t;
           r = strtok_r(NULL, ",", &s);
           }
     if (i) {
        filtersM = "";
        for (unsigned int j = 0; j < i; ++j)
            filtersM = cString::sprintf("%s%s%s", *filtersM, isempty(*filtersM) ? "" : ",", *cSource::ToString(sourceFiltersM[j]));
        debug3("%s filters=%s", __PRETTY_FUNCTION__, *filtersM);
        }
     FREE_POINTER(p);
     }
  if (!SatipConfig.GetDisableServerQuirks()) {
     // These devices contain a session id bug:
     // Inverto Airscreen Server IDL 400 ?
     // Elgato EyeTV Netstream 4Sat ?
     if (strstr(*descriptionM, "GSSBOX") ||                    // Grundig Sat Systems GSS.box DSI 400
         strstr(*descriptionM, "DIGIBIT") ||                   // Telestar Digibit R1
         strstr(*descriptionM, "Triax SatIP Converter")        // Triax TSS 400
        )
        quirkM |= eSatipQuirkSessionId;
     // These devices contain support for RTP over TCP:
     if (strstr(*descriptionM, "minisatip") ||                 // minisatip server
         strstr(*descriptionM, "DVBViewer")                    // DVBViewer Media Server
        )
        quirkM |= eSatipQuirkRtpOverTcp;
     // These devices contain a play (add/delpids) parameter bug:
     if (strstr(*descriptionM, "FRITZ!Box 6490 Cable") ||      // FRITZ!Box 6490 Cable
         strstr(*descriptionM, "FRITZ!WLAN Repeater DVB-C") || // FRITZ!WLAN Repeater DVB-C
         strstr(*descriptionM, "fritzdvbc")                    // FRITZ!WLAN Repeater DVB-C (old firmware)
        )
        quirkM |= eSatipQuirkPlayPids;
     // These devices contain a frontend locking bug:
     if (strstr(*descriptionM, "FRITZ!Box 6490 Cable") ||      // FRITZ!Box 6490 Cable
         strstr(*descriptionM, "FRITZ!WLAN Repeater DVB-C") || // FRITZ!WLAN Repeater DVB-C
         strstr(*descriptionM, "fritzdvbc") ||                 // FRITZ!WLAN Repeater DVB-C (old firmware)
         strstr(*descriptionM, "Schwaiger Sat>IP Server")      // Schwaiger MS41IP
        )
        quirkM |= eSatipQuirkForceLock;
     // These devices support the X_PMT protocol extension
     if (strstr(*descriptionM, "OctopusNet") ||                // Digital Devices OctopusNet
         strstr(*descriptionM, "minisatip")                    // minisatip server
        )
        quirkM |= eSatipQuirkCiXpmt;
     // These devices support the TNR protocol extension
     if (strstr(*descriptionM, "DVBViewer")                    // DVBViewer Media Server
        )
        quirkM |= eSatipQuirkCiTnr;
     // These devices don't support auto-detection of pilot tones
     if (strstr(*descriptionM, "GSSBOX") ||                    // Grundig Sat Systems GSS.box DSI 400
         strstr(*descriptionM, "DIGIBIT") ||                   // Telestar Digibit R1
         strstr(*descriptionM, "Triax SatIP Converter") ||     // Triax TSS 400
         strstr(*descriptionM, "KATHREIN SatIP Server")        // Kathrein ExIP 414/E
        )
        quirkM |= eSatipQuirkForcePilot;
     }
  if ((quirkM & eSatipQuirkMask) & eSatipQuirkSessionId)
     quirksM = cString::sprintf("%s%sSessionId", *quirksM, isempty(*quirksM) ? "" : ",");
  if ((quirkM & eSatipQuirkMask) & eSatipQuirkPlayPids)
     quirksM = cString::sprintf("%s%sPlayPids", *quirksM, isempty(*quirksM) ? "" : ",");
  if ((quirkM & eSatipQuirkMask) & eSatipQuirkForceLock)
     quirksM = cString::sprintf("%s%sForceLock", *quirksM, isempty(*quirksM) ? "" : ",");
  if ((quirkM & eSatipQuirkMask) & eSatipQuirkRtpOverTcp)
     quirksM = cString::sprintf("%s%sRtpOverTcp", *quirksM, isempty(*quirksM) ? "" : ",");
  if ((quirkM & eSatipQuirkMask) & eSatipQuirkCiXpmt)
     quirksM = cString::sprintf("%s%sCiXpmt", *quirksM, isempty(*quirksM) ? "" : ",");
  if ((quirkM & eSatipQuirkMask) & eSatipQuirkCiTnr)
     quirksM = cString::sprintf("%s%sCiTnr", *quirksM, isempty(*quirksM) ? "" : ",");
  if ((quirkM & eSatipQuirkMask) & eSatipQuirkForcePilot)
     quirksM = cString::sprintf("%s%sForcePilot", *quirksM, isempty(*quirksM) ? "" : ",");
  debug3("%s description=%s quirks=%s", __PRETTY_FUNCTION__, *descriptionM, *quirksM);
  // These devices support external CI
  if (strstr(*descriptionM, "OctopusNet") ||            // Digital Devices OctopusNet
      strstr(*descriptionM, "minisatip") ||             // minisatip server
      strstr(*descriptionM, "DVBViewer")                // DVBViewer Media Server
     ) {
     hasCiM = true;
     }
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
  FREE_POINTER(p);
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

bool cSatipServer::IsValidSource(int sourceP)
{
  if (sourceFiltersM[0]) {
     for (unsigned int i = 0; i < ELEMENTS(sourceFiltersM); ++i) {
         if (sourceP == sourceFiltersM[i]) {
            return true;
            }
         }
     return false;
     }
  return true;
}

bool cSatipServer::Assign(int deviceIdP, int sourceP, int systemP, int transponderP)
{
  bool result = false;
  if (IsValidSource(sourceP)) {
     if (cSource::IsType(sourceP, 'S'))
        result = frontendsM[eSatipFrontendDVBS2].Assign(deviceIdP, transponderP);
     else if (cSource::IsType(sourceP, 'T')) {
        if (systemP)
           result = frontendsM[eSatipFrontendDVBT2].Assign(deviceIdP, transponderP);
        else
           result = frontendsM[eSatipFrontendDVBT].Assign(deviceIdP, transponderP) || frontendsM[eSatipFrontendDVBT2].Assign(deviceIdP, transponderP);
        }
     else if (cSource::IsType(sourceP, 'C')) {
        if (systemP)
           result = frontendsM[eSatipFrontendDVBC2].Assign(deviceIdP, transponderP);
        else
           result = frontendsM[eSatipFrontendDVBC].Assign(deviceIdP, transponderP) || frontendsM[eSatipFrontendDVBC2].Assign(deviceIdP, transponderP);
        }
     }
  return result;
}

bool cSatipServer::Matches(int sourceP)
{
  if (IsValidSource(sourceP)) {
     if (cSource::IsType(sourceP, 'S'))
        return GetModulesDVBS2();
     else if (cSource::IsType(sourceP, 'T'))
        return GetModulesDVBT() || GetModulesDVBT2();
     else if (cSource::IsType(sourceP, 'C'))
        return GetModulesDVBC() || GetModulesDVBC2();
     }
  return false;
}

bool cSatipServer::Matches(int deviceIdP, int sourceP, int systemP, int transponderP)
{
  bool result = false;
  if (IsValidSource(sourceP)) {
     if (cSource::IsType(sourceP, 'S'))
        result = frontendsM[eSatipFrontendDVBS2].Matches(deviceIdP, transponderP);
     else if (cSource::IsType(sourceP, 'T')) {
        if (systemP)
           result = frontendsM[eSatipFrontendDVBT2].Matches(deviceIdP, transponderP);
        else
           result = frontendsM[eSatipFrontendDVBT].Matches(deviceIdP, transponderP) || frontendsM[eSatipFrontendDVBT2].Matches(deviceIdP, transponderP);
        }
     else if (cSource::IsType(sourceP, 'C')) {
        if (systemP)
           result = frontendsM[eSatipFrontendDVBC2].Matches(deviceIdP, transponderP);
        else
           result = frontendsM[eSatipFrontendDVBC].Matches(deviceIdP, transponderP) || frontendsM[eSatipFrontendDVBC2].Matches(deviceIdP, transponderP);
        }
     }
  return result;
}

void cSatipServer::Attach(int deviceIdP, int transponderP)
{
  for (int i = 0; i < eSatipFrontendCount; ++i) {
      if (frontendsM[i].Attach(deviceIdP, transponderP))
         return;
      }
}

void cSatipServer::Detach(int deviceIdP, int transponderP)
{
  for (int i = 0; i < eSatipFrontendCount; ++i) {
      if (frontendsM[i].Detach(deviceIdP, transponderP))
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

cSatipServer *cSatipServers::Assign(int deviceIdP, int sourceP, int transponderP, int systemP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s->IsActive() && s->Matches(deviceIdP, sourceP, systemP, transponderP))
         return s;
      }
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s->IsActive() && s->Assign(deviceIdP, sourceP, systemP, transponderP))
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

void cSatipServers::Activate(cSatipServer *serverP, bool onOffP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         s->Activate(onOffP);
         break;
         }
      }
}

void cSatipServers::Attach(cSatipServer *serverP, int deviceIdP, int transponderP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         s->Attach(deviceIdP, transponderP);
         break;
         }
      }
}

void cSatipServers::Detach(cSatipServer *serverP, int deviceIdP, int transponderP)
{
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         s->Detach(deviceIdP, transponderP);
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

cString cSatipServers::GetSrcAddress(cSatipServer *serverP)
{
  cString address = "";
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         address = s->SrcAddress();
         break;
         }
      }
  return address;
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

int cSatipServers::GetPort(cSatipServer *serverP)
{
  int port = SATIP_DEFAULT_RTSP_PORT;
  for (cSatipServer *s = First(); s; s = Next(s)) {
      if (s == serverP) {
         port = s->Port();
         break;
         }
      }
  return port;
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
      if (isempty(s->SrcAddress()))
         list = cString::sprintf("%s%c %s|%s|%s\n", *list, s->IsActive() ? '+' : '-', s->Address(), s->Model(), s->Description());
      else
         list = cString::sprintf("%s%c %s@%s|%s|%s\n", *list, s->IsActive() ? '+' : '-', s->SrcAddress(), s->Address(), s->Model(), s->Description());
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
