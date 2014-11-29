/*
 * rtcp.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "rtcp.h"

cSatipRtcp::cSatipRtcp(cSatipTunerIf &tunerP, unsigned int bufferLenP)
: tunerM(tunerP),
  bufferLenM(bufferLenP),
  bufferM(MALLOC(unsigned char, bufferLenM))
{
  debug("cSatipRtcp::%s(%u) [device %d]", __FUNCTION__, bufferLenP, tunerM.GetId());
  if (bufferM)
     memset(bufferM, 0, bufferLenM);
  else
     error("Cannot create RTCP buffer!");
}

cSatipRtcp::~cSatipRtcp()
{
  debug("cSatipRtcp::%s() [device %d]", __FUNCTION__, tunerM.GetId());
  DELETE_POINTER(bufferM);
}

int cSatipRtcp::GetFd(void)
{
  //debug("cSatipRtcp::%s() [device %d]", __FUNCTION__, tunerM.GetId());
  return Fd();
}

int cSatipRtcp::GetApplicationOffset(int *lengthP)
{
  //debug("cSatipRtcp::%s(%d) [device %d]", __FUNCTION__, *lengthP, tunerM.GetId());
  if (!lengthP)
     return -1;
  int offset = 0;
  int total = *lengthP;
  while (total > 0) {
        // Version
        unsigned int v = (bufferM[offset] >> 6) & 0x03;
         // Padding
        //unsigned int p = (bufferM[offset] >> 5) & 0x01;
        // Subtype
        //unsigned int st = bufferM[offset] & 0x1F;
        // Payload type
        unsigned int pt = bufferM[offset + 1] & 0xFF;
        // Lenght
        unsigned int length = ((bufferM[offset + 2] & 0xFF) << 8) | (bufferM[offset + 3] & 0xFF);
        // Convert it to bytes
        length = (length + 1) * 4;
        // V=2, APP = 204
        if ((v == 2) && (pt == 204)) {
           // SSCR/CSCR
           //unsigned int ssrc = ((bufferM[offset + 4] & 0xFF) << 24) | ((bufferM[offset + 5] & 0xFF) << 16) |
           //                     ((bufferM[offset + 6] & 0xFF) << 8) | (bufferM[offset + 7] & 0xFF);
           // Name
           if ((bufferM[offset +  8] == 'S') && (bufferM[offset +  9] == 'E') &&
               (bufferM[offset + 10] == 'S') && (bufferM[offset + 11] == '1')) {
              // Identifier
              //unsigned int id = ((bufferM[offset + 12] & 0xFF) << 8) | (bufferM[offset + 13] & 0xFF);
              // String length
              int string_length = ((bufferM[offset + 14] & 0xFF) << 8) | (bufferM[offset + 15] & 0xFF);
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
  //debug("cSatipRtcp::%s() [device %d]", __FUNCTION__, tunerM.GetId());
  if (bufferM) {
      int length;
      while ((length = Read(bufferM, bufferLenM)) > 0) {
            int offset = GetApplicationOffset(&length);
            if (offset >= 0)
               tunerM.ProcessApplicationData(bufferM + offset, length);
            }
      if (errno != EAGAIN && errno != EWOULDBLOCK)
         error("Error %d reading from RTCP socket [device %d]", errno, tunerM.GetId());
     }
}

cString cSatipRtcp::ToString(void) const
{
  return cString::sprintf("RTCP [device %d]", tunerM.GetId());
}
