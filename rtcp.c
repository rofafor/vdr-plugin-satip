/*
 * rtcp.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "common.h"
#include "rtcp.h"

cSatipRtcp::cSatipRtcp(cSatipTunerIf &tunerP, unsigned int bufferLenP)
: tunerM(&tunerP),
  bufferLenM(bufferLenP),
  bufferM(MALLOC(unsigned char, bufferLenM))
{
  if (bufferM)
     memset(bufferM, 0, bufferLenM);
  else
     error("Cannot create RTCP buffer!");
}

cSatipRtcp::~cSatipRtcp()
{
  DELETE_POINTER(bufferM);
}

int cSatipRtcp::GetFd(void)
{
  return Fd();
}

void cSatipRtcp::Action(int fdP)
{
  //debug("cSatipRtcp::%s(%d)", __FUNCTION__, fdP);
  if (bufferM) {
     int length = ReadApplication(bufferM, bufferLenM);
     if (length > 0)
        tunerM->ParseReceptionParameters(bufferM, length);
     }
}
