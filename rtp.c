/*
 * rtp.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "config.h"
#include "common.h"
#include "log.h"
#include "rtp.h"

cSatipRtp::cSatipRtp(cSatipTunerIf &tunerP, unsigned int bufferLenP)
: tunerM(tunerP),
  bufferLenM(bufferLenP),
  bufferM(MALLOC(unsigned char, bufferLenM)),
  lastErrorReportM(0),
  packetErrorsM(0),
  sequenceNumberM(-1)
{
  debug1("%s (, %u) [device %d]", __PRETTY_FUNCTION__, bufferLenP, tunerM.GetId());
  if (bufferM)
     memset(bufferM, 0, bufferLenM);
  else
     error("Cannot create RTP buffer!");
}

cSatipRtp::~cSatipRtp()
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  DELETE_POINTER(bufferM);
}

int cSatipRtp::GetFd(void)
{
  return Fd();
}

void cSatipRtp::Close(void)
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());

  cSatipSocket::Close();

  sequenceNumberM = -1;
  if (packetErrorsM) {
     info("Detected %d RTP packet errors [device %d]", packetErrorsM, tunerM.GetId());
     packetErrorsM = 0;
     lastErrorReportM = time(NULL);
     }
}

int cSatipRtp::GetHeaderLenght(unsigned int lengthP)
{
  debug8("%s (%d) [device %d]", __PRETTY_FUNCTION__, lengthP, tunerM.GetId());
  unsigned int headerlen = 0;

  if (lengthP > 0) {
     if (bufferM[0] == TS_SYNC_BYTE)
        return headerlen;
     else if (lengthP > 3) {
        // http://tools.ietf.org/html/rfc3550
        // http://tools.ietf.org/html/rfc2250
        // Version
        unsigned int v = (bufferM[0] >> 6) & 0x03;
        // Extension bit
        unsigned int x = (bufferM[0] >> 4) & 0x01;
        // CSCR count
        unsigned int cc = bufferM[0] & 0x0F;
        // Payload type: MPEG2 TS = 33
        //unsigned int pt = bufferAddrP[1] & 0x7F;
        // Sequence number
        int seq = ((bufferM[2] & 0xFF) << 8) | (bufferM[3] & 0xFF);
        if ((((sequenceNumberM + 1) % 0xFFFF) == 0) && (seq == 0xFFFF))
           sequenceNumberM = -1;
        else if ((sequenceNumberM >= 0) && (((sequenceNumberM + 1) % 0xFFFF) != seq)) {
           packetErrorsM++;
           if (time(NULL) - lastErrorReportM > eReportIntervalS) {
              info("Detected %d RTP packet errors [device %d]", packetErrorsM, tunerM.GetId());
              packetErrorsM = 0;
              lastErrorReportM = time(NULL);
              }
           sequenceNumberM = seq;
           }
        else
           sequenceNumberM = seq;
        // Header lenght
        headerlen = (3 + cc) * (unsigned int)sizeof(uint32_t);
        // Check if extension
        if (x) {
           // Extension header length
           unsigned int ehl = (((bufferM[headerlen + 2] & 0xFF) << 8) | (bufferM[headerlen + 3] & 0xFF));
           // Update header length
           headerlen += (ehl + 1) * (unsigned int)sizeof(uint32_t);
           }
        // Check for empty payload
        if (lengthP == headerlen) {
           debug1("%s (%d) Received empty RTP packet #%d [device %d]", __PRETTY_FUNCTION__, lengthP, seq, tunerM.GetId());
           headerlen = -1;
           }
        // Check that rtp is version 2 and payload contains multiple of TS packet data
        else if ((v != 2) || (((lengthP - headerlen) % TS_SIZE) != 0) || (bufferM[headerlen] != TS_SYNC_BYTE)) {
           debug1("%s (%d) Received incorrect RTP packet #%d v=%d len=%d sync=0x%02X [device %d]", __PRETTY_FUNCTION__, lengthP, seq, v, headerlen, bufferM[headerlen], tunerM.GetId());
           headerlen = -1;
           }
        }
     }

  return headerlen;
}

void cSatipRtp::Process(void)
{
  debug8("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  if (bufferM) {
     int length;
     while ((length = Read(bufferM, bufferLenM)) > 0) {
           int headerlen = GetHeaderLenght(length);
           if ((headerlen >= 0) && (headerlen < length))
               tunerM.ProcessVideoData(bufferM + headerlen, length - headerlen);
           }
     if (errno != EAGAIN && errno != EWOULDBLOCK)
        error("Error %d reading in %s [device %d]", errno, *ToString(), tunerM.GetId());
     }
}

cString cSatipRtp::ToString(void) const
{
  return cString::sprintf("RTP [device %d]", tunerM.GetId());
}
