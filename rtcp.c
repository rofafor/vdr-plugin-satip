/*
 * rtcp.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "config.h"
#include "common.h"
#include "log.h"
#include "rtcp.h"

cSatipRtcp::cSatipRtcp(cSatipTunerIf &tunerP)
: tunerM(tunerP),
  bufferLenM(eApplicationMaxSizeB),
  bufferM(MALLOC(unsigned char, bufferLenM))
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  if (bufferM)
     memset(bufferM, 0, bufferLenM);
  else
     error("Cannot create RTCP buffer! [device %d]", tunerM.GetId());
}

cSatipRtcp::~cSatipRtcp()
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  FREE_POINTER(bufferM);
}

int cSatipRtcp::GetFd(void)
{
  debug16("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  return Fd();
}

int cSatipRtcp::GetApplicationOffset(unsigned char *bufferP, int *lengthP)
{
  debug16("%s (%d) [device %d]", __PRETTY_FUNCTION__, lengthP ? *lengthP : -1, tunerM.GetId());
  if (!lengthP)
     return -1;
  int offset = 0;
  int total = *lengthP;
  while (total > 0) {
        // Version
        unsigned int v = (bufferP[offset] >> 6) & 0x03;
         // Padding
        //unsigned int p = (bufferP[offset] >> 5) & 0x01;
        // Subtype
        //unsigned int st = bufferP[offset] & 0x1F;
        // Payload type
        unsigned int pt = bufferP[offset + 1] & 0xFF;
        // Length
        unsigned int length = ((bufferP[offset + 2] & 0xFF) << 8) | (bufferP[offset + 3] & 0xFF);
        // Convert it to bytes
        length = (length + 1) * 4;
        // V=2, APP = 204
        if ((v == 2) && (pt == 204)) {
           // SSCR/CSCR
           //unsigned int ssrc = ((bufferP[offset + 4] & 0xFF) << 24) | ((bufferP[offset + 5] & 0xFF) << 16) |
           //                     ((bufferP[offset + 6] & 0xFF) << 8) | (bufferP[offset + 7] & 0xFF);
           // Name
           if ((bufferP[offset +  8] == 'S') && (bufferP[offset +  9] == 'E') &&
               (bufferP[offset + 10] == 'S') && (bufferP[offset + 11] == '1')) {
              // Identifier
              //unsigned int id = ((bufferP[offset + 12] & 0xFF) << 8) | (bufferP[offset + 13] & 0xFF);
              // String length
              int string_length = ((bufferP[offset + 14] & 0xFF) << 8) | (bufferP[offset + 15] & 0xFF);
              if (string_length > 0) {
                 *lengthP = string_length;
                 return (offset + 16);
                 }
              }
           }
        offset += length;
        total -= length;
        }
  *lengthP = 0;
  return -1;
}

void cSatipRtcp::Process(void)
{
  debug16("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  if (bufferM) {
     int length;
     while ((length = Read(bufferM, bufferLenM)) > 0) {
           int offset = GetApplicationOffset(bufferM, &length);
           if (offset >= 0)
              tunerM.ProcessApplicationData(bufferM + offset, length);
           }
     }
}

void cSatipRtcp::Process(unsigned char *dataP, int lengthP)
{
  debug16("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  if (dataP && lengthP > 0) {
     int offset = GetApplicationOffset(dataP, &lengthP);
     if (offset >= 0)
        tunerM.ProcessApplicationData(dataP + offset, lengthP);
     }
}

cString cSatipRtcp::ToString(void) const
{
  return cString::sprintf("RTCP [device %d]", tunerM.GetId());
}
