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

char *SkipZeroes(const char *strP)
{
  if ((uchar)*strP != '0')
     return (char *)strP;
  while (*strP && (uchar)*strP == '0')
        strP++;
  return (char *)strP;
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
  // description                        tag    pid   tid   mask
  {trNOOP("PAT (0x00)"),                "PAT", 0x00, 0x00, 0xFF},
  {trNOOP("NIT (0x40)"),                "NIT", 0x10, 0x40, 0xFF},
  {trNOOP("SDT (0x42)"),                "SDT", 0x11, 0x42, 0xFF},
  {trNOOP("EIT (0x4E/0x4F/0x5X/0x6X)"), "EIT", 0x12, 0x40, 0xC0},
  {trNOOP("TDT (0x70)"),                "TDT", 0x14, 0x70, 0xFF},
};

const ca_systems_table_type ca_systems_table[CA_SYSTEMS_TABLE_SIZE] =
{
  // http://www.dvb.org/index.php?id=174
  // http://en.wikipedia.org/wiki/Conditional_access_system
  // start end     description
  {0x0000, 0x0000, "---"                       }, // 0
  {0x0100, 0x01FF, "SECA Mediaguard (100..1FF)"}, // 1
  {0x0464, 0x0464, "EuroDec (464)"             }, // 2
  {0x0500, 0x05FF, "Viaccess (500..5FF)"       }, // 3
  {0x0600, 0x06FF, "Irdeto (600..6FF)"         }, // 4
  {0x0700, 0x07FF, "DigiCipher 2 (700..7FF)"   }, // 5
  {0x0900, 0x09FF, "NDS Videoguard (900..9FF)" }, // 6
  {0x0B00, 0x0BFF, "Conax (B00..BFF)"          }, // 7
  {0x0D00, 0x0DFF, "CryptoWorks (D00..DFF)"    }, // 8
  {0x0E00, 0x0EFF, "PowerVu (E00..EFF)"        }, // 9
  {0x1000, 0x10FF, "RAS (1000..10FF)"          }, // 10
  {0x1200, 0x12FF, "NagraVision (1200..12FF)"  }, // 11
  {0x1700, 0x17FF, "VCAS (1700..17FF)"         }, // 12
  {0x1800, 0x18FF, "NagraVision (1800..18FF)"  }, // 13
  {0x22F0, 0x22F0, "Codicrypt (22F0)"          }, // 14
  {0x2600, 0x2600, "BISS (2600)"               }, // 15
  {0x2719, 0x2719, "VanyaCas (2719)"           }, // 16
  {0x4347, 0x4347, "CryptOn (4347)"            }, // 17
  {0x4800, 0x4800, "Accessgate (4800)"         }, // 18
  {0x4900, 0x4900, "China Crypt (4900)"        }, // 19
  {0x4A02, 0x4A02, "Tongfang (4A02)"           }, // 20
  {0x4A10, 0x4A10, "EasyCas (4A10)"            }, // 21
  {0x4A20, 0x4A20, "AlphaCrypt (4A20)"         }, // 22
  {0x4A60, 0x4A60, "SkyCrypt (4A60)"           }, // 23
  {0x4A61, 0x4A61, "Neotioncrypt (4A61)"       }, // 24
  {0x4A62, 0x4A62, "SkyCrypt (4A62)"           }, // 25
  {0x4A63, 0x4A63, "Neotion SHL (4A63)"        }, // 26
  {0x4A64, 0x4A6F, "SkyCrypt (4A64)"           }, // 27
  {0x4A70, 0x4A70, "DreamCrypt (4A70)"         }, // 28
  {0x4A80, 0x4A80, "ThalesCrypt (4A80)"        }, // 29
  {0x4AA1, 0x4AA1, "KeyFly (4AA1)"             }, // 30
  {0x4ABF, 0x4ABF, "CTI-CAS (4ABF)"            }, // 31
  {0x4AC1, 0x4AC1, "Latens (4AC1)"             }, // 32
  {0x4AD0, 0x4AD1, "X-Crypt (4AD0)"            }, // 33
  {0x4AD4, 0x4AD4, "OmniCrypt (4AD4)"          }, // 34
  {0x4AE0, 0x4AE1, "Z-Crypt (4AE0)"            }, // 35
  {0x4AE4, 0x4AE4, "CoreCrypt (4AE4)"          }, // 36
  {0x4AE5, 0x4AE5, "PRO-Crypt (4AE5)"          }, // 37
  {0x4AEA, 0x4AEA, "Cryptoguard (4AEA)"        }, // 38
  {0x4AEB, 0x4AEB, "Abel Quintic (4AEB)"       }, // 39
  {0x4AF0, 0x4AF0, "ABV (4AF0)"                }, // 40
  {0x5500, 0x5500, "Z-Crypt (5500)"            }, // 41
  {0x5501, 0x5501, "Griffin (5501)"            }, // 42
  {0x5581, 0x5581, "Bulcrypt (5581)"           }, // 43
  {0x7BE1, 0x7BE1, "DRE-Crypt (7BE1)"          }, // 44
  {0xA101, 0xA101, "RosCrypt-M (A101)"         }, // 45
  {0xEAD0, 0xEAD0, "VanyaCas (EAD0)"           }, // 46
};

bool checkCASystem(unsigned int cicamP, int caidP)
{
  // always skip the first row
  if ((cicamP > 0) && (cicamP < ELEMENTS(ca_systems_table)))
     return ((caidP >= ca_systems_table[cicamP].start) && (caidP <= ca_systems_table[cicamP].end));
  return false;
}
