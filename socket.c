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
#include "log.h"
#include "socket.h"

cSatipSocket::cSatipSocket()
: socketPortM(0),
  socketDescM(-1)
{
  debug1("%s", __PRETTY_FUNCTION__);
  memset(&sockAddrM, 0, sizeof(sockAddrM));
}

cSatipSocket::~cSatipSocket()
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Close the socket
  Close();
}

bool cSatipSocket::Open(const int portP)
{
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
  debug1("%s (%d) socketPort=%d", __PRETTY_FUNCTION__, portP, socketPortM);
  return true;
}

void cSatipSocket::Close(void)
{
  debug1("%s sockerPort=%d", __PRETTY_FUNCTION__, socketPortM);
  // Check if socket exists
  if (socketDescM >= 0) {
     close(socketDescM);
     socketDescM = -1;
     socketPortM = 0;
     memset(&sockAddrM, 0, sizeof(sockAddrM));
     }
}

bool cSatipSocket::Flush(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
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
  debug16("%s (, %d)", __PRETTY_FUNCTION__, bufferLenP);
  // Error out if socket not initialized
  if (socketDescM <= 0) {
     error("%s Invalid socket", __PRETTY_FUNCTION__);
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
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_flags = 0;

    if (socketDescM && bufferAddrP && (bufferLenP > 0))
       len = (int)recvmsg(socketDescM, &msgh, MSG_DONTWAIT);
    if (len > 0)
       return len;
    } while (len > 0);
  ERROR_IF_RET(len < 0 && errno != EAGAIN && errno != EWOULDBLOCK, "recvmsg()", return -1);
  return 0;
}

int cSatipSocket::ReadMulti(unsigned char *bufferAddrP, unsigned int *elementRecvSizeP, unsigned int elementCountP, unsigned int elementBufferSizeP)
{
  debug16("%s (, , %d, %d)", __PRETTY_FUNCTION__, elementCountP, elementBufferSizeP);
  int count = -1;
  // Error out if socket not initialized
  if (socketDescM <= 0) {
     error("%s Invalid socket", __PRETTY_FUNCTION__);
     return -1;
     }
  if (!bufferAddrP || !elementRecvSizeP || !elementCountP || !elementBufferSizeP) {
     error("%s Invalid parameter(s)", __PRETTY_FUNCTION__);
     return -1;
     }
#if defined(__GLIBC_PREREQ) && __GLIBC_PREREQ(2,12)
  // Initialize iov and msgh structures
  struct mmsghdr mmsgh[elementCountP];
  struct iovec iov[elementCountP];
  memset(mmsgh, 0, sizeof(mmsgh[0]) * elementCountP);
  for (unsigned int i = 0; i < elementCountP; ++i) {
      iov[i].iov_base = bufferAddrP + i * elementBufferSizeP;
      iov[i].iov_len = elementBufferSizeP;
      mmsgh[i].msg_hdr.msg_iov = &iov[i];
      mmsgh[i].msg_hdr.msg_iovlen = 1;
      }

  // Read data from socket as a set
  count = (int)recvmmsg(socketDescM, mmsgh, elementCountP, MSG_DONTWAIT, NULL);
  ERROR_IF_RET(count < 0 && errno != EAGAIN && errno != EWOULDBLOCK, "recvmmsg()", return -1);
  for (int i = 0; i < count; ++i)
      elementRecvSizeP[i] = mmsgh[i].msg_len;
#else
  count = 0;
  while (count < (int)elementCountP) {
        int len = Read(bufferAddrP + count * elementBufferSizeP, elementBufferSizeP);
        if (len < 0)
           return -1;
        else if (len == 0)
           break;
        elementRecvSizeP[count++] = len;
        }
#endif
  debug16("%s Received %d packets size[0]=%d", __PRETTY_FUNCTION__, count, elementRecvSizeP[0]);

  return count;
}


bool cSatipSocket::Write(const char *addrP, const unsigned char *bufferAddrP, unsigned int bufferLenP)
{
  debug1("%s (%s, , %d)", __PRETTY_FUNCTION__, addrP, bufferLenP);
  // Error out if socket not initialized
  if (socketDescM <= 0) {
     error("%s Invalid socket", __PRETTY_FUNCTION__);
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
