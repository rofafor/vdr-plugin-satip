/*
 * rtp.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "rtp.h"

cSatipRtp::cSatipRtp(cSatipDeviceIf &deviceP, unsigned int bufferLenP)
: deviceM(&deviceP),
  bufferLenM(bufferLenP),
  bufferM(MALLOC(unsigned char, bufferLenM))
{
  if (bufferM)
     memset(bufferM, 0, bufferLenM);
  else
     error("Cannot create RTP buffer!");
}

cSatipRtp::~cSatipRtp()
{
  DELETE_POINTER(bufferM);
}

int cSatipRtp::GetFd(void)
{
  return Fd();
}

void cSatipRtp::Action(int fdP)
{
  //debug("cSatipRtp::%s(%d)", __FUNCTION__, fdP);
  if (bufferM) {
     int length = ReadVideo(bufferM, min(deviceM->CheckData(), bufferLenM));
     if (length > 0)
        deviceM->WriteData(bufferM, length);
     }
}
