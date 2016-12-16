/*
 * msearch.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "config.h"
#include "common.h"
#include "discover.h"
#include "log.h"
#include "poller.h"
#include "msearch.h"

const char *cSatipMsearch::bcastAddressS = "239.255.255.250";
const char *cSatipMsearch::bcastMessageS = "M-SEARCH * HTTP/1.1\r\n"                  \
                                           "HOST: 239.255.255.250:1900\r\n"           \
                                           "MAN: \"ssdp:discover\"\r\n"               \
                                           "ST: urn:ses-com:device:SatIPServer:1\r\n" \
                                           "MX: 2\r\n\r\n";

cSatipMsearch::cSatipMsearch(cSatipDiscoverIf &discoverP)
: discoverM(discoverP),
  bufferLenM(eProbeBufferSize),
  bufferM(MALLOC(unsigned char, bufferLenM)),
  registeredM(false)
{
  if (bufferM)
     memset(bufferM, 0, bufferLenM);
  else
     error("Cannot create Msearch buffer!");
  if (!Open(eDiscoveryPort, true))
     error("Cannot open Msearch port!");
}

cSatipMsearch::~cSatipMsearch()
{
  FREE_POINTER(bufferM);
}

void cSatipMsearch::Probe(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (!registeredM) {
     cSatipPoller::GetInstance()->Register(*this);
     registeredM = true;
     }
  // Send two queries with one second interval
  Write(bcastAddressS, reinterpret_cast<const unsigned char *>(bcastMessageS), strlen(bcastMessageS));
  cCondWait::SleepMs(1000);
  Write(bcastAddressS, reinterpret_cast<const unsigned char *>(bcastMessageS), strlen(bcastMessageS));
}

int cSatipMsearch::GetFd(void)
{
  return Fd();
}

void cSatipMsearch::Process(void)
{
  debug16("%s", __PRETTY_FUNCTION__);
  if (bufferM) {
     int length;
     while ((length = Read(bufferM, bufferLenM)) > 0) {
           bufferM[min(length, int(bufferLenM - 1))] = 0;
           debug13("%s len=%d buf=%s", __PRETTY_FUNCTION__, length, bufferM);
           bool status = false, valid = false;
           char *s, *p = reinterpret_cast<char *>(bufferM), *location = NULL;
           char *r = strtok_r(p, "\r\n", &s);
           while (r) {
                 debug13("%s r=%s", __PRETTY_FUNCTION__, r);
                 // Check the status code
                 // HTTP/1.1 200 OK
                 if (!status && startswith(r, "HTTP/1.1 200 OK"))
                    status = true;
                 if (status) {
                    // Check the location data
                    // LOCATION: http://192.168.0.115:8888/octonet.xml
                    if (startswith(r, "LOCATION:")) {
                       location = compactspace(r + 9);
                       debug1("%s location='%s'", __PRETTY_FUNCTION__, location);
                       }
                    // Check the source type
                    // ST: urn:ses-com:device:SatIPServer:1
                    else if (startswith(r, "ST:")) {
                       char *st = compactspace(r + 3);
                       if (strstr(st, "urn:ses-com:device:SatIPServer:1"))
                          valid = true;
                       debug1("%s st='%s'", __PRETTY_FUNCTION__, st);
                       }
                    // Check whether all the required data is found
                    if (valid && !isempty(location)) {
                       discoverM.SetUrl(location);
                       break;
                       }
                    }
                 r = strtok_r(NULL, "\r\n", &s);
                 }
           }
     }
}

void cSatipMsearch::Process(unsigned char *dataP, int lengthP)
{
  debug16("%s", __PRETTY_FUNCTION__);
}

cString cSatipMsearch::ToString(void) const
{
  return "MSearch";
}
