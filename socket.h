/*
 * socket.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_SOCKET_H
#define __SATIP_SOCKET_H

#include <arpa/inet.h>

class cSatipSocket {
private:
  int socketPortM;
  int socketDescM;
  struct sockaddr_in sockAddrM;
  bool isMulticastM;
  bool useSsmM;
  in_addr_t streamAddrM;
  in_addr_t sourceAddrM;
  size_t rcvBufSizeM;

  bool CheckAddress(const char *addrP, in_addr_t *inAddrP);
  bool Join(void);
  bool Leave(void);

public:
  cSatipSocket();
  explicit cSatipSocket(size_t rcvBufSizeP);
  virtual ~cSatipSocket();
  bool Open(const int portP = 0, const bool reuseP = false);
  bool OpenMulticast(const int portP, const char *streamAddrP, const char *sourceAddrP);
  virtual void Close(void);
  int Fd(void) { return socketDescM; }
  int Port(void) { return socketPortM; }
  bool IsMulticast(void) { return isMulticastM; }
  bool IsOpen(void) { return (socketDescM >= 0); }
  bool Flush(void);
  int Read(unsigned char *bufferAddrP, unsigned int bufferLenP);
  int ReadMulti(unsigned char *bufferAddrP, unsigned int *elementRecvSizeP, unsigned int elementCountP, unsigned int elementBufferSizeP);
  bool Write(const char *addrP, const unsigned char *bufferAddrP, unsigned int bufferLenP);
};

#endif // __SATIP_SOCKET_H

