/*
 * tuner.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_DATA_H
#define __SATIP_DATA_H

#include <curl/curl.h>
#include <curl/easy.h>

#ifndef CURLOPT_RTSPHEADER
#error "libcurl is missing required RTSP support"
#endif

#include <vdr/thread.h>
#include <vdr/tools.h>

#include "deviceif.h"
#include "statistics.h"
#include "socket.h"

class cSatipTunerDataThread: public cThread {
public:
  typedef void (*fCallback)(void *parm);
 
  cSatipTunerDataThread(cSatipDeviceIf &deviceP, cSatipTunerStatistics &statisticsP, unsigned int packetLenP);
  ~cSatipTunerDataThread(void);
  void Start(cSatipSocket *rtpSocketP);
  void SetTimeout(int timeoutP, fCallback callbackP, void *parmP);
  void Cancel(int WaitSeconds = 0);
  void Flush();

protected:
  void Action(void);
  
private:
  cSatipTunerDataThread(cSatipTunerDataThread &toCopy); // Prohibit copying 

  cSatipDeviceIf *deviceM;
  cSatipTunerStatistics *statisticsM;
  unsigned int packetBufferLenM;
  unsigned char *packetBufferM;
  cSatipSocket *rtpSocketM;
  int timeoutM;
  cTimeMs timeoutHandlerM;
  fCallback timeoutFuncM;
  void *timeoutParamM;
  cCondWait sleepM;
  cMutex mutexM;
};

#endif // __SATIP_DATA_H
