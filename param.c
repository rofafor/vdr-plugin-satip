/*
 * param.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <ctype.h>
#include <vdr/dvbdevice.h>
#include "common.h"
#include "param.h"

// --- cSatipParameterMaps ----------------------------------------------------

struct tSatipParameterMap {
  int         driverValue;
  const char *satipString;
};

static const tSatipParameterMap SatipBandwidthValues[] = {
  {  5000000, "bw=5"     },
  {  6000000, "bw=6"     },
  {  7000000, "bw=7"     },
  {  8000000, "bw=8"     },
  { 10000000, "bw=10"    },
  {  1712000, "bw=1.712" },
  {       -1, NULL       }
};

static const tSatipParameterMap SatipPilotValues[] = {
  {  PILOT_OFF, "plts=off" },
  {   PILOT_ON, "plts=on"  },
  { PILOT_AUTO, ""         },
  {         -1, NULL       }
};

static const tSatipParameterMap SatipSisoMisoValues[] = {
  {   0, "sm=0" },
  {   1, "sm=1" },
  {  -1, NULL   }
};

static const tSatipParameterMap SatipCodeRateValues[] = {
  { FEC_NONE, ""        },
  {  FEC_1_2, "fec=12"  },
  {  FEC_2_3, "fec=23"  },
  {  FEC_3_4, "fec=34"  },
  {  FEC_3_5, "fec=35"  },
  {  FEC_4_5, "fec=45"  },
  {  FEC_5_6, "fec=56"  },
  {  FEC_6_7, "fec=67"  },
  {  FEC_7_8, "fec=78"  },
  {  FEC_8_9, "fec=89"  },
  { FEC_9_10, "fec=910" },
  { FEC_AUTO, ""        },
  {       -1, NULL      }
};

static const tSatipParameterMap SatipModulationValues[] = {
  {     QPSK, "mtype=qpsk"   },
  {    PSK_8, "mtype=8psk"   },
  {   QAM_16, "mtype=16qam"  },
  {   QAM_64, "mtype=64qam"  },
  {  QAM_256, "mtype=256qam" },
  { QAM_AUTO, ""             },
  {       -1, NULL           }
};

static const tSatipParameterMap SatipSystemValuesSat[] = {
  {   0, "msys=dvbs"  },
  {   1, "msys=dvbs2" },
  {  -1, NULL         }
};

static const tSatipParameterMap SatipSystemValuesTerr[] = {
  {   0, "msys=dvbt"  },
  {   1, "msys=dvbt2" },
  {  -1, NULL         }
};

static const tSatipParameterMap SatipTransmissionValues[] = {
  {   TRANSMISSION_MODE_1K, "tmode=1k"  },
  {   TRANSMISSION_MODE_2K, "tmode=2k"  },
  {   TRANSMISSION_MODE_4K, "tmode=4k"  },
  {   TRANSMISSION_MODE_8K, "tmode=8k"  },
  {  TRANSMISSION_MODE_16K, "tmode=16k" },
  {  TRANSMISSION_MODE_32K, "tmode=32k" },
  { TRANSMISSION_MODE_AUTO, ""          },
  {                     -1, NULL        }
};

static const tSatipParameterMap SatipGuardValues[] = {
  {    GUARD_INTERVAL_1_4, "gi=14"    },
  {    GUARD_INTERVAL_1_8, "gi=18"    },
  {   GUARD_INTERVAL_1_16, "gi=116"   },
  {   GUARD_INTERVAL_1_32, "gi=132"   },
  {  GUARD_INTERVAL_1_128, "gi=1128"  },
  { GUARD_INTERVAL_19_128, "gi=19128" },
  { GUARD_INTERVAL_19_256, "gi=19256" },
  {   GUARD_INTERVAL_AUTO, ""         },
  {                    -1, NULL       }
};

static const tSatipParameterMap SatipRollOffValues[] = {
  { ROLLOFF_AUTO, ""        },
  {   ROLLOFF_20, "ro=0.20" },
  {   ROLLOFF_25, "ro=0.25" },
  {   ROLLOFF_35, "ro=0.35" },
  {           -1, NULL      }
};

static int SatipUserIndex(int valueP, const tSatipParameterMap *mapP)
{
  const tSatipParameterMap *map = mapP;
  while (map && map->driverValue != -1) {
        if (map->driverValue == valueP)
           return map - mapP;
        map++;
        }
  return -1;
}

static int PrintUrlString(char *ptrP, int valueP, const tSatipParameterMap *mapP)
{
  int n = SatipUserIndex(valueP, mapP);
  return (n >= 0) ? sprintf(ptrP, "&%s", tr(mapP[n].satipString)) : 0;
}

cString GetTransponderUrlParameters(const cChannel *channelP)
{
  if (channelP) {
     cDvbTransponderParameters dtp(channelP->Parameters());
     int Pilot = PILOT_AUTO; // should be added into cDvbTransponderParameters
     int T2SystemId = 0;     // should be added into cDvbTransponderParameters
     int SisoMiso = 0;       // should be added into cDvbTransponderParameters
     float freq = channelP->Frequency();
     char type = cSource::ToChar(channelP->Source());
     cSource *source = Sources.Get(channelP->Source());
     int src = (strchr("S", type) && source) ? atoi(source->Description()) : 1;
     char buffer[255];
     char *q = buffer;
     *q = 0;
     // Scale down frequencies to MHz
     while (freq > 20000L)
           freq /= 1000L;
     q += sprintf(q, "freq=%s", *dtoa(freq, "%.3f"));
#define ST(s) if (strchr(s, type) && (strchr(s, '0' + dtp.System() + 1) || strchr(s, '*')))
     ST("S *") q += sprintf(q,        "&src=%d",          ((src > 0) && (src <= 255)) ? src : 1);
     ST("S *") q += sprintf(q,        "&sr=%d",           channelP->Srate());
     ST("S *") q += sprintf(q,        "&pol=%c",          tolower(dtp.Polarization()));
     ST(" T2") q += sprintf(q,        "&plp=%d",          dtp.StreamId());
     ST(" T2") q += sprintf(q,        "&t2id=%d",         T2SystemId);
     ST(" T2") q += PrintUrlString(q, SisoMiso,           SatipSisoMisoValues);
     ST(" T*") q += PrintUrlString(q, dtp.Bandwidth(),    SatipBandwidthValues);
     ST(" T*") q += PrintUrlString(q, dtp.Guard(),        SatipGuardValues);
     ST("ST*") q += PrintUrlString(q, dtp.CoderateH(),    SatipCodeRateValues);
     ST("S 2") q += PrintUrlString(q, Pilot,              SatipPilotValues);
     ST("S 2") q += PrintUrlString(q, dtp.Modulation(),   SatipModulationValues);
     ST(" T*") q += PrintUrlString(q, dtp.Modulation(),   SatipModulationValues);
     ST("S 2") q += PrintUrlString(q, dtp.RollOff(),      SatipRollOffValues);
     ST("S *") q += PrintUrlString(q, dtp.System(),       SatipSystemValuesSat);
     ST(" T*") q += PrintUrlString(q, dtp.System(),       SatipSystemValuesTerr);
     ST(" T*") q += PrintUrlString(q, dtp.Transmission(), SatipTransmissionValues);
#undef ST
     return buffer;
     }
  return NULL;
}
