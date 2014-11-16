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

public:
  cSatipSocket();
  virtual ~cSatipSocket();
  bool Open(const int portP = 0);
  virtual void Close(void);
  int Fd(void) { return socketDescM; }
  int Port(void) { return socketPortM; }
  bool IsOpen(void) { return (socketDescM >= 0); }
  bool Flush(void);
  int Read(unsigned char *bufferAddrP, unsigned int bufferLenP);
  bool Write(const char *addrP, const unsigned char *bufferAddrP, unsigned int bufferLenP);
};

#endif // __SATIP_SOCKET_H

