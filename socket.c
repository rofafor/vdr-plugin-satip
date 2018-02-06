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

#if defined(__GLIBC__)
  #if defined(__GLIBC_PREREQ)
    #if !__GLIBC_PREREQ(2,12)
      #define __SATIP_DISABLE_RECVMMSG__
    #endif
  #endif
#endif

cSatipSocket::cSatipSocket()
: socketPortM(0),
  socketDescM(-1),
  isMulticastM(false),
  useSsmM(false),
  streamAddrM(htonl(INADDR_ANY)),
  sourceAddrM(htonl(INADDR_ANY)),
  rcvBufSizeM(0)
{
  debug1("%s", __PRETTY_FUNCTION__);
  memset(&sockAddrM, 0, sizeof(sockAddrM));
}

cSatipSocket::cSatipSocket(size_t rcvBufSizeP)
: socketPortM(0),
  socketDescM(-1),
  isMulticastM(false),
  useSsmM(false),
  streamAddrM(htonl(INADDR_ANY)),
  sourceAddrM(htonl(INADDR_ANY)),
  rcvBufSizeM(rcvBufSizeP)
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

bool cSatipSocket::Open(const int portP, const bool reuseP)
{
  // If socket is there already and it is bound to a different port, it must
  // be closed first
  if (portP != socketPortM) {
     debug1("%s (%d, %d) Socket tear-down", __PRETTY_FUNCTION__, portP, reuseP);
     Close();
     }
  // Bind to the socket if it is not active already
  if (socketDescM < 0) {
     int yes;
     socklen_t len = sizeof(sockAddrM);
     // Create socket
     socketDescM = socket(PF_INET, SOCK_DGRAM, 0);
     ERROR_IF_RET(socketDescM < 0, "socket()", return false);
     // Make it use non-blocking I/O to avoid stuck read calls
     ERROR_IF_FUNC(fcntl(socketDescM, F_SETFL, O_NONBLOCK), "fcntl(O_NONBLOCK)",
                   Close(), return false);
     // Allow multiple sockets to use the same PORT number
     yes = reuseP;
     ERROR_IF_FUNC(setsockopt(socketDescM, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0,
                   "setsockopt(SO_REUSEADDR)", Close(), return false);
     yes = reuseP;
#ifdef SO_REUSEPORT
     ERROR_IF_FUNC(setsockopt(socketDescM, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) < 0 && errno != ENOPROTOOPT,
                   "setsockopt(SO_REUSEPORT)", Close(), return false);
#endif
#ifndef __FreeBSD__
     // Allow packet information to be fetched
     ERROR_IF_FUNC(setsockopt(socketDescM, SOL_IP, IP_PKTINFO, &yes, sizeof(yes)) < 0,
                   "setsockopt(IP_PKTINFO)", Close(), return false);
#endif // __FreeBSD__
     // Tweak receive buffer size if requested
     if (rcvBufSizeM > 0) {
        ERROR_IF_FUNC(setsockopt(socketDescM, SOL_SOCKET, SO_RCVBUF, &rcvBufSizeM, sizeof(rcvBufSizeM)) < 0,
                      "setsockopt(SO_RCVBUF)", Close(), return false);
     }
     // Bind socket
     memset(&sockAddrM, 0, sizeof(sockAddrM));
     sockAddrM.sin_family = AF_INET;
     sockAddrM.sin_port = htons((uint16_t)(portP & 0xFFFF));
     sockAddrM.sin_addr.s_addr = htonl(INADDR_ANY);
     ERROR_IF_FUNC(bind(socketDescM, (struct sockaddr *)&sockAddrM, sizeof(sockAddrM)) < 0,
                   "bind()", Close(), return false);
     // Update socket port
     ERROR_IF_FUNC(getsockname(socketDescM, (struct sockaddr*)&sockAddrM, &len) < 0,
                   "getsockname()", Close(), return false);
     socketPortM = ntohs(sockAddrM.sin_port);
     isMulticastM = false;
     }
  debug1("%s (%d) socketPort=%d", __PRETTY_FUNCTION__, portP, socketPortM);
  return true;
}

bool cSatipSocket::OpenMulticast(const int portP, const char *streamAddrP, const char *sourceAddrP)
{
  debug1("%s (%d, %s, %s)", __PRETTY_FUNCTION__, portP, streamAddrP, sourceAddrP);
  if (Open(portP)) {
     CheckAddress(streamAddrP, &streamAddrM);
     if (!isempty(sourceAddrP))
        useSsmM = CheckAddress(sourceAddrP, &sourceAddrM);
     return Join();
     }
  return false;
}

void cSatipSocket::Close(void)
{
  debug1("%s socketPort=%d", __PRETTY_FUNCTION__, socketPortM);
  // Check if socket exists
  if (socketDescM >= 0) {
     Leave();
     close(socketDescM);
     socketDescM = -1;
     socketPortM = 0;
     memset(&sockAddrM, 0, sizeof(sockAddrM));
     streamAddrM = htonl(INADDR_ANY);
     sourceAddrM = htonl(INADDR_ANY);
     isMulticastM = false;
     useSsmM = false;
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

bool cSatipSocket::CheckAddress(const char *addrP, in_addr_t *inAddrP)
{
  if (inAddrP) {
     // First try only the IP address
     *inAddrP = inet_addr(addrP);
     if (*inAddrP == htonl(INADDR_NONE)) {
        debug1("%s (%s, ) Cannot convert to address", __PRETTY_FUNCTION__, addrP);
        // It may be a host name, get the name
        struct hostent *host = gethostbyname(addrP);
        if (!host) {
           char tmp[64];
           error("gethostbyname() failed: %s is not valid address: %s", addrP,
                 strerror_r(h_errno, tmp, sizeof(tmp)));
           return false;
           }
        *inAddrP = inet_addr(*host->h_addr_list);
        }
     return true;
     }
  return false;
}

bool cSatipSocket::Join(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Check if socket exists
  if (socketDescM >= 0 && !isMulticastM) {
     // Join a new multicast group
     if (useSsmM) {
        // Source-specific multicast (SSM) is used
        struct group_source_req gsr;
        struct sockaddr_in *grp;
        struct sockaddr_in *src;
        gsr.gsr_interface = 0; // if_nametoindex("any") ?
        grp = (struct sockaddr_in*)&gsr.gsr_group;
        grp->sin_family = AF_INET;
        grp->sin_addr.s_addr = streamAddrM;
        grp->sin_port = 0;
        src = (struct sockaddr_in*)&gsr.gsr_source;
        src->sin_family = AF_INET;
        src->sin_addr.s_addr = sourceAddrM;
        src->sin_port = 0;
        ERROR_IF_RET(setsockopt(socketDescM, SOL_IP, MCAST_JOIN_SOURCE_GROUP, &gsr, sizeof(gsr)) < 0, "setsockopt(MCAST_JOIN_SOURCE_GROUP)", return false);
        }
     else {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = streamAddrM;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        ERROR_IF_RET(setsockopt(socketDescM, SOL_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0, "setsockopt(IP_ADD_MEMBERSHIP)", return false);
        }
     // Update multicasting flag
     isMulticastM = true;
     }
  return true;
}

bool cSatipSocket::Leave(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Check if socket exists
  if (socketDescM >= 0 && isMulticastM) {
     // Leave the existing multicast group
     if (useSsmM) {
        // Source-specific multicast (SSM) is used
        struct group_source_req gsr;
        struct sockaddr_in *grp;
        struct sockaddr_in *src;
        gsr.gsr_interface = 0; // if_nametoindex("any") ?
        grp = (struct sockaddr_in*)&gsr.gsr_group;
        grp->sin_family = AF_INET;
        grp->sin_addr.s_addr = streamAddrM;
        grp->sin_port = 0;
        src = (struct sockaddr_in*)&gsr.gsr_source;
        src->sin_family = AF_INET;
        src->sin_addr.s_addr = sourceAddrM;
        src->sin_port = 0;
        ERROR_IF_RET(setsockopt(socketDescM, SOL_IP, MCAST_LEAVE_SOURCE_GROUP, &gsr, sizeof(gsr)) < 0, "setsockopt(MCAST_LEAVE_SOURCE_GROUP)", return false);
        }
     else {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = streamAddrM;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        ERROR_IF_RET(setsockopt(socketDescM, SOL_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0, "setsockopt(IP_DROP_MEMBERSHIP)", return false);
        }
     // Update multicasting flag
     isMulticastM = false;
     }
  return true;
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
    if (len > 0) {
#ifndef __FreeBSD__
       if (isMulticastM) {
          // Process auxiliary received data and validate source address
          for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL; cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
              if ((cmsg->cmsg_level == SOL_IP) && (cmsg->cmsg_type == IP_PKTINFO)) {
                 struct in_pktinfo *i = (struct in_pktinfo *)CMSG_DATA(cmsg);
                 if ((i->ipi_addr.s_addr == streamAddrM) || (htonl(INADDR_ANY) == streamAddrM))
                    return len;
                 }
              }
          }
       else
#endif // __FreeBSD__
          return len;
       }
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
#ifndef __SATIP_DISABLE_RECVMMSG__
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
