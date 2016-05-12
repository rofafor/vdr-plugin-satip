/*
 * common.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_COMMON_H
#define __SATIP_COMMON_H

#include <vdr/device.h>
#include <vdr/tools.h>
#include <vdr/config.h>
#include <vdr/i18n.h>

#define SATIP_DEFAULT_RTSP_PORT          554

#define SATIP_MAX_DEVICES                MAXDEVICES

#define SATIP_BUFFER_SIZE                KILOBYTE(2048)

#define SATIP_DEVICE_INFO_ALL            0
#define SATIP_DEVICE_INFO_GENERAL        1
#define SATIP_DEVICE_INFO_PIDS           2
#define SATIP_DEVICE_INFO_FILTERS        3
#define SATIP_DEVICE_INFO_PROTOCOL       4
#define SATIP_DEVICE_INFO_BITRATE        5

#define SATIP_STATS_ACTIVE_PIDS_COUNT    10
#define SATIP_STATS_ACTIVE_FILTERS_COUNT 10

#define MAX_DISABLED_SOURCES_COUNT       25
#define SECTION_FILTER_TABLE_SIZE        5

#define MAX_CICAM_COUNT                  2
#define CA_SYSTEMS_TABLE_SIZE            47

#define SATIP_CURL_EASY_GETINFO(X, Y, Z) \
  if ((res = curl_easy_getinfo((X), (Y), (Z))) != CURLE_OK) { \
     esyslog("curl_easy_getinfo(%s) [%s,%d] failed: %s (%d)", #Y,  __FILE__, __LINE__, curl_easy_strerror(res), res); \
     }

#define SATIP_CURL_EASY_SETOPT(X, Y, Z) \
  if ((res = curl_easy_setopt((X), (Y), (Z))) != CURLE_OK) { \
     esyslog("curl_easy_setopt(%s, %s) [%s,%d] failed: %s (%d)", #Y, #Z, __FILE__, __LINE__, curl_easy_strerror(res), res); \
     }

#define SATIP_CURL_EASY_PERFORM(X) \
  if ((res = curl_easy_perform((X))) != CURLE_OK) { \
     esyslog("curl_easy_perform() [%s,%d] failed: %s (%d)",  __FILE__, __LINE__, curl_easy_strerror(res), res); \
     }

#define ERROR_IF_FUNC(exp, errstr, func, ret)                  \
  do {                                                         \
     if (exp) {                                                \
        char tmp[64];                                          \
        esyslog("[%s,%d]: " errstr ": %s", __FILE__, __LINE__, \
                strerror_r(errno, tmp, sizeof(tmp)));          \
        func;                                                  \
        ret;                                                   \
        }                                                      \
  } while (0)


#define ERROR_IF_RET(exp, errstr, ret) ERROR_IF_FUNC(exp, errstr, ,ret);

#define ERROR_IF(exp, errstr) ERROR_IF_FUNC(exp, errstr, , );

#define DELETE_POINTER(ptr)      \
  do {                           \
     if (ptr) {                  \
        typeof(*ptr) *tmp = ptr; \
        ptr = NULL;              \
        delete(tmp);             \
        }                        \
  } while (0)

#define FREE_POINTER(ptr)        \
  do {                           \
     if (ptr) {                  \
        typeof(*ptr) *tmp = ptr; \
        ptr = NULL;              \
        free(tmp);               \
        }                        \
  } while (0)

#define ELEMENTS(x) (sizeof(x) / sizeof(x[0]))

class cSatipMemoryBuffer {
private:
  enum {
    eMaxDataSize = MEGABYTE(2)
  };
  char *dataM;
  size_t sizeM;
  void *AllocBuffer(void *ptrP, size_t sizeP)
  {
    // There might be a realloc() out there that doesn't like reallocing NULL pointers, so we take care of it here
    if (ptrP)
       return realloc(ptrP, sizeP);
    else
       return malloc(sizeP);
  }
  // to prevent copy constructor and assignment
  cSatipMemoryBuffer(const cSatipMemoryBuffer&);
  cSatipMemoryBuffer& operator=(const cSatipMemoryBuffer&);
public:
  cSatipMemoryBuffer() : dataM(NULL), sizeM(0) {}
  ~cSatipMemoryBuffer() { Reset(); }
  size_t Add(char *dataP, size_t sizeP)
  {
     if (sizeP > 0) {
        size_t len = sizeM + sizeP + 1;
        if (len < eMaxDataSize) {
           dataM = (char *)AllocBuffer(dataM, len);
           if (dataM) {
              memcpy(&(dataM[sizeM]), dataP, sizeP);
              sizeM += sizeP;
              dataM[sizeM] = 0;
              return sizeP;
              }
           else
              esyslog("[%s,%d]: Failed to allocate memory", __FILE__, __LINE__);
          }
       else
          esyslog("[%s,%d]: Buffer overflow", __FILE__, __LINE__);
       }
     return 0;
  };
  char *Data(void) { return dataM; }
  size_t Size(void) { return sizeM; }
  void Reset(void) { FREE_POINTER(dataM); sizeM = 0; };
};

uint16_t ts_pid(const uint8_t *bufP);
uint8_t payload(const uint8_t *bufP);
const char *id_pid(const u_short pidP);
char *StripTags(char *strP);
char *SkipZeroes(const char *strP);
cString ChangeCase(const cString &strP, bool upperP);

struct section_filter_table_type {
  const char *description;
  const char *tag;
  u_short pid;
  u_char tid;
  u_char mask;
};

extern const section_filter_table_type section_filter_table[SECTION_FILTER_TABLE_SIZE];

struct ca_systems_table_type {
  int start;
  int end;
  const char *description;
};

extern const ca_systems_table_type ca_systems_table[CA_SYSTEMS_TABLE_SIZE];
extern bool checkCASystem(unsigned int cicamP, int caidP);

extern const char VERSION[];

#endif // __SATIP_COMMON_H

