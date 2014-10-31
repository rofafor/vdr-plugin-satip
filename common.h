/*
 * common.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_COMMON_H
#define __SATIP_COMMON_H

#include <vdr/tools.h>
#include <vdr/config.h>
#include <vdr/i18n.h>

#ifdef DEBUG
#define debug(x...) dsyslog("SATIP: " x);
#define info(x...)  isyslog("SATIP: " x);
#define error(x...) esyslog("ERROR: " x);
#else
#define debug(x...) ;
#define info(x...)  isyslog("SATIP: " x);
#define error(x...) esyslog("ERROR: " x);
#endif

#define ELEMENTS(x)                      (sizeof(x) / sizeof(x[0]))

#define SATIP_BUFFER_SIZE                MEGABYTE(1)

#define SATIP_DEVICE_INFO_ALL            0
#define SATIP_DEVICE_INFO_GENERAL        1
#define SATIP_DEVICE_INFO_PIDS           2
#define SATIP_DEVICE_INFO_FILTERS        3
#define SATIP_DEVICE_INFO_PROTOCOL       4
#define SATIP_DEVICE_INFO_BITRATE        5

#define SATIP_STATS_ACTIVE_PIDS_COUNT    10
#define SATIP_STATS_ACTIVE_FILTERS_COUNT 10

#define MAX_DISABLED_SOURCES_COUNT       5
#define SECTION_FILTER_TABLE_SIZE        5

#define SATIP_CURL_EASY_GETINFO(X, Y, Z) \
  if ((res = curl_easy_getinfo((X), (Y), (Z))) != CURLE_OK) { \
     error("curl_easy_getinfo(%s) [%s,%d] failed: %s (%d)", #Y,  __FILE__, __LINE__, curl_easy_strerror(res), res); \
     }

#define SATIP_CURL_EASY_SETOPT(X, Y, Z) \
  if ((res = curl_easy_setopt((X), (Y), (Z))) != CURLE_OK) { \
     error("curl_easy_setopt(%s, %s) [%s,%d] failed: %s (%d)", #Y, #Z, __FILE__, __LINE__, curl_easy_strerror(res), res); \
     }

#define SATIP_CURL_EASY_PERFORM(X) \
  if ((res = curl_easy_perform((X))) != CURLE_OK) { \
     error("curl_easy_perform() [%s,%d] failed: %s (%d)",  __FILE__, __LINE__, curl_easy_strerror(res), res); \
     }

#define ERROR_IF_FUNC(exp, errstr, func, ret)              \
  do {                                                     \
     if (exp) {                                            \
        char tmp[64];                                      \
        error("[%s,%d]: "errstr": %s", __FILE__, __LINE__, \
              strerror_r(errno, tmp, sizeof(tmp)));        \
        func;                                              \
        ret;                                               \
        }                                                  \
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

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

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

extern const char VERSION[];

#endif // __SATIP_COMMON_H

