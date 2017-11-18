/*
 * rtp.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>

#include "config.h"
#include "common.h"
#include "log.h"
#include "rtp.h"

cSatipRtp::cSatipRtp(cSatipTunerIf &tunerP)
: cSatipSocket(SatipConfig.GetRtpRcvBufSize()),
  tunerM(tunerP),
  bufferLenM(eRtpPacketReadCount * eMaxUdpPacketSizeB),
  bufferM(MALLOC(unsigned char, bufferLenM)),
  lastErrorReportM(0),
  packetErrorsM(0),
  sequenceNumberM(-1)
{
  debug1("%s () [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  if (!bufferM)
     error("Cannot create RTP buffer! [device %d]", tunerM.GetId());
}

cSatipRtp::~cSatipRtp()
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  FREE_POINTER(bufferM);
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
     info("Detected %d RTP packet error%s [device %d]", packetErrorsM, packetErrorsM == 1 ? "": "s", tunerM.GetId());
     packetErrorsM = 0;
     lastErrorReportM = time(NULL);
     }
}

int cSatipRtp::GetHeaderLength(unsigned char *bufferP, unsigned int lengthP)
{
  debug16("%s (, %d) [device %d]", __PRETTY_FUNCTION__, lengthP, tunerM.GetId());
  unsigned int headerlen = 0;

  if (lengthP > 0) {
     if (bufferP[0] == TS_SYNC_BYTE)
        return headerlen;
     else if (lengthP > 3) {
        // http://tools.ietf.org/html/rfc3550
        // http://tools.ietf.org/html/rfc2250
        // Version
        unsigned int v = (bufferP[0] >> 6) & 0x03;
        // Extension bit
        unsigned int x = (bufferP[0] >> 4) & 0x01;
        // CSCR count
        unsigned int cc = bufferP[0] & 0x0F;
        // Payload type: MPEG2 TS = 33
        unsigned int pt = bufferP[1] & 0x7F;
        if (pt != 33)
           debug7("%s (%d) Received invalid RTP payload type %d - v=%d [device %d]",
                    __PRETTY_FUNCTION__, lengthP, pt, v, tunerM.GetId());
        // Sequence number
        int seq = ((bufferP[2] & 0xFF) << 8) | (bufferP[3] & 0xFF);
        if ((((sequenceNumberM + 1) % 0xFFFF) == 0) && (seq == 0xFFFF))
           sequenceNumberM = -1;
        else if ((sequenceNumberM >= 0) && (((sequenceNumberM + 1) % 0xFFFF) != seq)) {
           packetErrorsM++;
           if (time(NULL) - lastErrorReportM > eReportIntervalS) {
              info("Detected %d RTP packet error%s [device %d]", packetErrorsM, packetErrorsM == 1 ? "": "s", tunerM.GetId());
              packetErrorsM = 0;
              lastErrorReportM = time(NULL);
              }
           sequenceNumberM = seq;
           }
        else
           sequenceNumberM = seq;
        // Header length
        headerlen = (3 + cc) * (unsigned int)sizeof(uint32_t);
        // Check if extension
        if (x) {
           // Extension header length
           unsigned int ehl = (((bufferP[headerlen + 2] & 0xFF) << 8) | (bufferP[headerlen + 3] & 0xFF));
           // Update header length
           headerlen += (ehl + 1) * (unsigned int)sizeof(uint32_t);
           }
        // Check for empty payload
        if (lengthP == headerlen) {
           debug7("%s (%d) Received empty RTP packet #%d [device %d]", __PRETTY_FUNCTION__, lengthP, seq, tunerM.GetId());
           headerlen = -1;
           }
        // Check that rtp is version 2 and payload contains multiple of TS packet data
        else if ((v != 2) || (((lengthP - headerlen) % TS_SIZE) != 0) || (bufferP[headerlen] != TS_SYNC_BYTE)) {
           debug7("%s (%d) Received incorrect RTP packet #%d v=%d len=%d sync=0x%02X [device %d]", __PRETTY_FUNCTION__,
                   lengthP, seq, v, headerlen, bufferP[headerlen], tunerM.GetId());
           headerlen = -1;
           }
        else
           debug7("%s (%d) Received RTP packet #%d v=%d len=%d sync=0x%02X [device %d]", __PRETTY_FUNCTION__,
                   lengthP, seq, v, headerlen, bufferP[headerlen], tunerM.GetId());
        }
     }

  return headerlen;
}

void cSatipRtp::Process(void)
{
  debug16("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  if (bufferM) {
     unsigned int lenMsg[eRtpPacketReadCount];
     uint64_t elapsed;
     int count = 0;
     cTimeMs processing(0);

     do {
       count = ReadMulti(bufferM, lenMsg, eRtpPacketReadCount, eMaxUdpPacketSizeB);
       for (int i = 0; i < count; ++i) {
           unsigned char *p = &bufferM[i * eMaxUdpPacketSizeB];
           int headerlen = GetHeaderLength(p, lenMsg[i]);
           if ((headerlen >= 0) && (headerlen < (int)lenMsg[i]))
              tunerM.ProcessVideoData(p + headerlen, lenMsg[i] - headerlen);
           }
       } while (count >= eRtpPacketReadCount);

     elapsed = processing.Elapsed();
     if (elapsed > 1)
        debug6("%s %d read(s) took %" PRIu64 " ms [device %d]", __PRETTY_FUNCTION__, count, elapsed, tunerM.GetId());
     }
}

void cSatipRtp::Process(unsigned char *dataP, int lengthP)
{
  debug16("%s [device %d]", __PRETTY_FUNCTION__, tunerM.GetId());
  if (dataP && lengthP > 0) {
     uint64_t elapsed;
     cTimeMs processing(0);
     int headerlen = GetHeaderLength(dataP, lengthP);
     if ((headerlen >= 0) && (headerlen < lengthP))
        tunerM.ProcessVideoData(dataP + headerlen, lengthP - headerlen);

     elapsed = processing.Elapsed();
     if (elapsed > 1)
        debug6("%s %d read(s) took %" PRIu64 " ms [device %d]", __PRETTY_FUNCTION__, lengthP, elapsed, tunerM.GetId());
     }
}

cString cSatipRtp::ToString(void) const
{
  return cString::sprintf("RTP [device %d]", tunerM.GetId());
}
