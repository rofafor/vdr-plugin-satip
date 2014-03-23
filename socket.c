/*
 * socket.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#include <vdr/device.h>

#include "common.h"
#include "config.h"
#include "socket.h"

cSatipSocket::cSatipSocket()
: socketPortM(0),
  socketDescM(-1),
  sequenceNumberM(-1)
{
  debug("cSatipSocket::%s()", __FUNCTION__);
  memset(&sockAddrM, 0, sizeof(sockAddrM));
}

cSatipSocket::~cSatipSocket()
{
  debug("cSatipSocket::%s()", __FUNCTION__);
  // Close the socket
  Close();
}

bool cSatipSocket::Open(const int portP)
{
  debug("cSatipSocket::%s(%d)", __FUNCTION__, portP);
  // Bind to the socket if it is not active already
  if (socketDescM < 0) {
     socklen_t len = sizeof(sockAddrM);
     // Create socket
     socketDescM = socket(PF_INET, SOCK_DGRAM, 0);
     ERROR_IF_RET(socketDescM < 0, "socket()", return false);
     // Make it use non-blocking I/O to avoid stuck read calls
     ERROR_IF_FUNC(fcntl(socketDescM, F_SETFL, O_NONBLOCK), "fcntl(O_NONBLOCK)",
                   Close(), return false);
     // Allow multiple sockets to use the same PORT number
     int yes = 1;
     ERROR_IF_FUNC(setsockopt(socketDescM, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0,
                   "setsockopt(SO_REUSEADDR)", Close(), return false);
     // Bind socket
     memset(&sockAddrM, 0, sizeof(sockAddrM));
     sockAddrM.sin_family = AF_INET;
     sockAddrM.sin_port = htons((uint16_t)(portP & 0xFFFF));
     sockAddrM.sin_addr.s_addr = htonl(INADDR_ANY);
     ERROR_IF_FUNC(bind(socketDescM, (struct sockaddr *)&sockAddrM, sizeof(sockAddrM)) < 0,
                   "bind()", Close(), return false);
     // Update socket port
     ERROR_IF_FUNC(getsockname(socketDescM,(struct sockaddr*)&sockAddrM, &len) < 0,
                   "getsockname()", Close(), return false);
     socketPortM = ntohs(sockAddrM.sin_port);
     }
  return true;
}

void cSatipSocket::Close(void)
{
  debug("cSatipSocket::%s()", __FUNCTION__);
  // Check if socket exists
  if (socketDescM >= 0) {
     close(socketDescM);
     socketDescM = -1;
     socketPortM = 0;
     sequenceNumberM = -1;
     memset(&sockAddrM, 0, sizeof(sockAddrM));
     }
}

bool cSatipSocket::Flush(void)
{
  debug("cSatipSocket::%s()", __FUNCTION__);
  if (socketDescM < 0) {
     const unsigned int len = 65535;
     unsigned char *buf = MALLOC(unsigned char, len);
     if (buf) {
        int i = 0;
        do {
           // Sanity check
           if (++i > 10)
              break;
        } while (Read(buf, len));
        return true;
        }
     }
  return false;
}

int cSatipSocket::Read(unsigned char *bufferAddrP, unsigned int bufferLenP)
{
  //debug("cSatipSocket::%s()", __FUNCTION__);
  // Error out if socket not initialized
  if (socketDescM <= 0) {
     error("Invalid socket in cSatipUdpSocket::%s()", __FUNCTION__);
     return -1;
     }
  int len = 0;
  // Read data from socket in a loop
  do {
    socklen_t addrlen = sizeof(sockAddrM);
    struct msghdr msgh;
    struct iovec iov;
    char cbuf[256];
    len = 0;
    // Initialize iov and msgh structures
    memset(&msgh, 0, sizeof(struct msghdr));
    iov.iov_base = bufferAddrP;
    iov.iov_len = bufferLenP;
    msgh.msg_control = cbuf;
    msgh.msg_controllen = sizeof(cbuf);
    msgh.msg_name = &sockAddrM;
    msgh.msg_namelen = addrlen;
    msgh.msg_iov  = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_flags = 0;

    if (socketDescM && bufferAddrP && (bufferLenP > 0))
       len = (int)recvmsg(socketDescM, &msgh, MSG_DONTWAIT);
    if (len > 0)
       return len;
    } while (len > 0);
  ERROR_IF_RET(len < 0 && errno != EAGAIN, "recvmsg()", return -1);
  return 0;
}

int cSatipSocket::ReadVideo(unsigned char *bufferAddrP, unsigned int bufferLenP)
{
  //debug("cSatipSocket::%s()", __FUNCTION__);
  int len = Read(bufferAddrP, bufferLenP);
  if (len > 0) {
     if (bufferAddrP[0] == TS_SYNC_BYTE)
        return len;
     else if (len > 3) {
        // http://tools.ietf.org/html/rfc3550
        // http://tools.ietf.org/html/rfc2250
        // Version
        unsigned int v = (bufferAddrP[0] >> 6) & 0x03;
        // Extension bit
        unsigned int x = (bufferAddrP[0] >> 4) & 0x01;
        // CSCR count
        unsigned int cc = bufferAddrP[0] & 0x0F;
        // Payload type: MPEG2 TS = 33
        //unsigned int pt = bufferAddrP[1] & 0x7F;
        // Sequence number
        int seq = ((bufferAddrP[2] & 0xFF) << 8) | (bufferAddrP[3] & 0xFF);
        if ((sequenceNumberM >= 0) && (((sequenceNumberM + 1) % 0xFFFF) != seq))
           error("missed %d RTP packets", seq - sequenceNumberM - 1);
        sequenceNumberM = seq;
        // Header lenght
        unsigned int headerlen = (3 + cc) * (unsigned int)sizeof(uint32_t);
        // Check if extension
        if (x) {
           // Extension header length
           unsigned int ehl = (((bufferAddrP[headerlen + 2] & 0xFF) << 8) |
                               (bufferAddrP[headerlen + 3] & 0xFF));
           // Update header length
           headerlen += (ehl + 1) * (unsigned int)sizeof(uint32_t);
           }
        // Check that rtp is version 2 and payload contains multiple of TS packet data
        if ((v == 2) && (((len - headerlen) % TS_SIZE) == 0) &&
            (bufferAddrP[headerlen] == TS_SYNC_BYTE)) {
           // Set argument point to payload in read buffer
           memmove(bufferAddrP, &bufferAddrP[headerlen], (len - headerlen));
           return (len - headerlen);
           }
        }
     }
  return 0;
}

int cSatipSocket::ReadApplication(unsigned char *bufferAddrP, unsigned int bufferLenP)
{
  //debug("cSatipSocket::%s()", __FUNCTION__);
  int len = Read(bufferAddrP, bufferLenP);
  int offset = 0;
  while (len > 0) {
        // Version
        unsigned int v = (bufferAddrP[offset] >> 6) & 0x03;
         // Padding
        //unsigned int p = (bufferAddrP[offset] >> 5) & 0x01;
        // Subtype
        //unsigned int st = bufferAddrP[offset] & 0x1F;
        // Payload type
        unsigned int pt = bufferAddrP[offset + 1] & 0xFF;
        // Lenght
        unsigned int length = ((bufferAddrP[offset + 2] & 0xFF) << 8) | (bufferAddrP[offset + 3] & 0xFF);
        // Convert it to bytes
        length = (length + 1) * 4;
        // V=2, APP = 204
        if ((v == 2) && (pt == 204)) {
           // SSCR/CSCR
           //unsigned int ssrc = ((bufferAddrP[offset + 4] & 0xFF) << 24) | ((bufferAddrP[offset + 5] & 0xFF) << 16) |
           //                     ((bufferAddrP[offset + 6] & 0xFF) << 8) | (bufferAddrP[offset + 7] & 0xFF);
           // Name
           if ((bufferAddrP[offset +  8] == 'S') && (bufferAddrP[offset +  9] == 'E') &&
               (bufferAddrP[offset + 10] == 'S') && (bufferAddrP[offset + 11] == '1')) {
              // Identifier
              //unsigned int id = ((bufferAddrP[offset + 12] & 0xFF) << 8) | (bufferAddrP[offset + 13] & 0xFF);
              // String length
              int string_length = ((bufferAddrP[offset + 14] & 0xFF) << 8) | (bufferAddrP[offset + 15] & 0xFF);
              if (string_length > 0) {
                 // Set argument point to payload in read buffer
                 memmove(bufferAddrP, &bufferAddrP[offset + 16], string_length);
                 return string_length;
                 }
              }
           }
        offset += length;
        len -= length;
        }
  return 0;
}

bool cSatipSocket::Write(const char *addrP, const unsigned char *bufferAddrP, unsigned int bufferLenP)
{
  debug("cSatipSocket::%s(%s)", __FUNCTION__, addrP);
  // Error out if socket not initialized
  if (socketDescM <= 0) {
     error("cSatipSocket::%s(): Invalid socket", __FUNCTION__);
     return false;
     }
  struct sockaddr_in sockAddr;
  memset(&sockAddr, 0, sizeof(sockAddr));
  sockAddr.sin_family = AF_INET;
  sockAddr.sin_port = htons((uint16_t)(socketPortM & 0xFFFF));
  sockAddr.sin_addr.s_addr = inet_addr(addrP);
  ERROR_IF_RET(sendto(socketDescM, bufferAddrP, bufferLenP, 0, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) < 0, "sendto()", return false);
  return true;
}
