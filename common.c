/*
 * common.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <ctype.h>
#include <vdr/tools.h>
#include "common.h"

uint16_t ts_pid(const uint8_t *bufP)
{
  return (uint16_t)(((bufP[1] & 0x1f) << 8) + bufP[2]);
}

uint8_t payload(const uint8_t *bufP)
{
  if (!(bufP[3] & 0x10)) // no payload?
     return 0;

  if (bufP[3] & 0x20) {  // adaptation field?
     if (bufP[4] > 183)  // corrupted data?
        return 0;
     else
        return (uint8_t)((184 - 1) - bufP[4]);
     }

  return 184;
}

const char *id_pid(const u_short pidP)
{
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i) {
      if (pidP == section_filter_table[i].pid)
         return section_filter_table[i].tag;
      }
  return "---";
}

char *StripTags(char *strP)
{
  if (strP) {
     char *c = strP, *r = strP, t = 0;
     while (*strP) {
           if (*strP == '<')
              ++t;
           else if (*strP == '>')
              --t;
           else if (t < 1)
              *(c++) = *strP;
           ++strP;
           }
     *c = 0;
     return r;
     }
  return NULL;
}

cString ChangeCase(const cString &strP, bool upperP)
{
  cString res(strP);
  char *p = (char *)*res;
  while (p && *p) {
        *p = upperP ? toupper(*p) : tolower(*p);
        ++p;
        }
  return res;
}

const section_filter_table_type section_filter_table[SECTION_FILTER_TABLE_SIZE] =
{
  /* description              tag    pid   tid   mask */
  {trNOOP("PAT (0x00)"),      "PAT", 0x00, 0x00, 0xFF},
  {trNOOP("NIT (0x40)"),      "NIT", 0x10, 0x40, 0xFF},
  {trNOOP("SDT (0x42)"),      "SDT", 0x11, 0x42, 0xFF},
  {trNOOP("EIT (0x4E/0x4F)"), "EIT", 0x12, 0x4E, 0xFE},
  {trNOOP("EIT (0x5X)"),      "EIT", 0x12, 0x50, 0xF0},
  {trNOOP("EIT (0x6X)"),      "EIT", 0x12, 0x60, 0xF0},
  {trNOOP("TDT (0x70)"),      "TDT", 0x14, 0x70, 0xFF},
};
