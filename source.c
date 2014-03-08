/*
 * source.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <ctype.h>
#include "common.h"
#include "source.h"

// --- cSatipParameterMaps ----------------------------------------------------

static const tSatipParameterMap SatipBandwidthValues[] = {
  {    5, "5 MHz",     "bw=5"     },
  {    6, "6 MHz",     "bw=6"     },
  {    7, "7 MHz",     "bw=7"     },
  {    8, "8 MHz",     "bw=8"     },
  {   10, "10 MHz",    "bw=10"    },
  { 1712, "1.712 MHz", "bw=1.712" },
  {   -1, NULL,        NULL       }
};

static const tSatipParameterMap SatipPilotTonesValues[] = {
  {   0, trNOOP("off"),  "plts=off" },
  {   1, trNOOP("on"),   "plts=on"  },
  { 999, trNOOP("auto"), ""         },
  {  -1, NULL,           NULL       }
};

static const tSatipParameterMap SatipSisoMisoValues[] = {
  {   0, trNOOP("SISO"), "sm=0" },
  {   1, trNOOP("MISO"), "sm=1" },
  { 999, trNOOP("auto"), ""     },
  {  -1, NULL,           NULL   }
};

static const tSatipParameterMap SatipCodeRateValues[] = {
  {   0, trNOOP("none"), ""        },
  {  12, "1/2",          "fec=12"  },
  {  23, "2/3",          "fec=23"  },
  {  34, "3/4",          "fec=34"  },
  {  35, "3/5",          "fec=35"  },
  {  45, "4/5",          "fec=45"  },
  {  56, "5/6",          "fec=56"  },
  {  78, "7/8",          "fec=78"  },
  {  89, "8/9",          "fec=89"  },
  { 910, "9/10",         "fec=910" },
  { 999, trNOOP("auto"), ""        },
  {  -1, NULL,           NULL      }
};

static const tSatipParameterMap SatipModulationValues[] = {
  {   2, "QPSK",         "mtype=qpsk"   },
  {   5, "8PSK",         "mtype=8psk"   },
  {  16, "QAM16",        "mtype=16qam"  },
  {  64, "QAM64",        "mtype=64qam"  },
  { 256, "QAM256",       "mtype=256qam" },
  { 999, trNOOP("auto"), ""             },
  {  -1, NULL,           NULL           }
};

static const tSatipParameterMap SatipSystemValuesSat[] = {
  {   0, "DVB-S",  "msys=dvbs"  },
  {   1, "DVB-S2", "msys=dvbs2" },
  {  -1, NULL,     NULL         }
};

static const tSatipParameterMap SatipSystemValuesTerr[] = {
  {   0, "DVB-T",  "msys=dvbt"  },
  {   1, "DVB-T2", "msys=dvbt2" },
  {  -1, NULL,     NULL         }
};

static const tSatipParameterMap SatipTransmissionValues[] = {
  {   1, "1K",           "tmode=1k"  },
  {   2, "2K",           "tmode=2k"  },
  {   4, "4K",           "tmode=4k"  },
  {   8, "8K",           "tmode=8k"  },
  {  16, "16K",          "tmode=16k" },
  {  32, "32K",          "tmode=32k" },
  { 999, trNOOP("auto"), ""          },
  {  -1, NULL,           NULL        }
};

static const tSatipParameterMap SatipGuardValues[] = {
  {     4, "1/4",          "gi=14"    },
  {     8, "1/8",          "gi=18"    },
  {    16, "1/16",         "gi=116"   },
  {    32, "1/32",         "gi=132"   },
  {   128, "1/128",        "gi=1128"  },
  { 19128, "19/128",       "gi=19128" },
  { 19256, "19/256",       "gi=19256" },
  {   999, trNOOP("auto"), ""         },
  {    -1, NULL,           NULL       }
};

static const tSatipParameterMap SatipRollOffValues[] = {
  {  0, trNOOP("auto"), ""        },
  { 20, "0.20",         "ro=0.20" },
  { 25, "0.25",         "ro=0.25" },
  { 35, "0.35",         "ro=0.35" },
  { -1, NULL,            NULL     }
};

static int SatipUserIndex(int valueP, const tSatipParameterMap *mapP)
{
  const tSatipParameterMap *map = mapP;
  while (map && map->userValue != -1) {
        if (map->userValue == valueP)
           return map - mapP;
        map++;
        }
  return -1;
}

static int SatipMapToUser(int valueP, const tSatipParameterMap *mapP, const char **strP)
{
  int n = SatipUserIndex(valueP, mapP);
  if (n >= 0) {
     if (strP)
        *strP = tr(mapP[n].userString);
     return mapP[n].userValue;
     }
  return -1;
}

// --- cMenuEditSatipItem -----------------------------------------------------

class cMenuEditSatipItem : public cMenuEditItem {
protected:
  int *valueM;
  const tSatipParameterMap *mapM;
  const char *zeroStringM;
  virtual void Set(void);

public:
  cMenuEditSatipItem(const char *nameP, int *valueP, const tSatipParameterMap *mapP, const char *zeroStringP = NULL);
  virtual eOSState ProcessKey(eKeys keyP);
};

cMenuEditSatipItem::cMenuEditSatipItem(const char *nameP, int *valueP, const tSatipParameterMap *mapP, const char *zeroStringP)
: cMenuEditItem(nameP)
{
  valueM = valueP;
  mapM = mapP;
  zeroStringM = zeroStringP;
  Set();
}

void cMenuEditSatipItem::Set(void)
{
  const char *s = NULL;
  int n = SatipMapToUser(*valueM, mapM, &s);
  if (n == 0 && zeroStringM)
     SetValue(zeroStringM);
  else if (n >= 0) {
     if (s)
        SetValue(s);
     else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", n);
        SetValue(buf);
        }
     }
  else
     SetValue("???");
}

eOSState cMenuEditSatipItem::ProcessKey(eKeys keyP)
{
  eOSState state = cMenuEditItem::ProcessKey(keyP);

  if (state == osUnknown) {
     int newValue = *valueM;
     int n = SatipUserIndex(*valueM, mapM);
     if (NORMALKEY(keyP) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (n-- > 0)
           newValue = mapM[n].userValue;
        }
     else if (NORMALKEY(keyP) == kRight) {
        if (mapM[++n].userValue >= 0)
           newValue = mapM[n].userValue;
        }
     else
        return state;
     if (newValue != *valueM) {
        *valueM = newValue;
        Set();
        }
     state = osContinue;
     }
  return state;
}

// --- cSatipTransponderParameters --------------------------------------------

cSatipTransponderParameters::cSatipTransponderParameters(const char *parametersP)
: polarizationM('H'),
  bandwidthM(8),
  coderateHM(0),
  systemM(0),
  modulationM(999),
  transmissionM(8),
  guardM(999),
  rollOffM(0),
  streamIdM(0),
  t2SystemIdM(0),
  sisoMisoM(999),
  pilotTonesM(0),
  signalSourceM(1)
{
  Parse(parametersP);
}

int cSatipTransponderParameters::PrintParameter(char *ptrP, char nameP, int valueP) const
{
  return (valueP >= 0 && valueP != 999) ? sprintf(ptrP, "%c%d", nameP, valueP) : 0;
}

int cSatipTransponderParameters::PrintString(char *ptrP, int valueP, const tSatipParameterMap *mapP)
{
  int n = SatipUserIndex(valueP, mapP);
  return (n >= 0) ? sprintf(ptrP, "&%s", tr(mapP[n].satipString)) : 0;
}

cString cSatipTransponderParameters::UrlParameters(char typeP)
{
  char buffer[255];
  char *q = buffer;
  *q = 0;
#define ST(s) if (strchr(s, typeP) && (strchr(s, '0' + systemM + 1) || strchr(s, '*')))
  ST("Z *") q += sprintf(q,     "&src=%d",     signalSourceM);
  ST("Z *") q += sprintf(q,     "&pol=%c",     tolower(polarizationM));
  ST(" Y2") q += sprintf(q,     "&plp=%d",     streamIdM);
  ST(" Y2") q += sprintf(q,     "&t2id=%d",    t2SystemIdM);
  ST(" Y*") q += PrintString(q, bandwidthM,    SatipBandwidthValues);
  ST(" Y*") q += PrintString(q, guardM,        SatipGuardValues);
  ST("ZY*") q += PrintString(q, coderateHM,    SatipCodeRateValues);
  ST("Z 2") q += PrintString(q, pilotTonesM,   SatipPilotTonesValues);
  ST("Z 2") q += PrintString(q, modulationM,   SatipModulationValues);
  ST(" Y*") q += PrintString(q, modulationM,   SatipModulationValues);
  ST("Z 2") q += PrintString(q, rollOffM,      SatipRollOffValues);
  ST("Z *") q += PrintString(q, systemM,       SatipSystemValuesSat);
  ST(" Y*") q += PrintString(q, systemM,       SatipSystemValuesTerr);
  ST(" Y*") q += PrintString(q, transmissionM, SatipTransmissionValues);
  ST(" Y2") q += PrintString(q, sisoMisoM,     SatipSisoMisoValues);
#undef ST
  return buffer;
}

cString cSatipTransponderParameters::ToString(char typeP) const
{
  char buffer[64];
  char *q = buffer;
  *q = 0;
#define ST(s) if (strchr(s, typeP) && (strchr(s, '0' + systemM + 1) || strchr(s, '*')))
  ST("Z *") q += sprintf(q, "%c", polarizationM);
  ST(" Y*") q += PrintParameter(q, 'B', bandwidthM);
  ST("ZY*") q += PrintParameter(q, 'C', coderateHM);
  ST(" Y*") q += PrintParameter(q, 'G', guardM);
  ST("Z 2") q += PrintParameter(q, 'M', modulationM);
  ST(" Y*") q += PrintParameter(q, 'M', modulationM);
  ST("Z 2") q += PrintParameter(q, 'N', pilotTonesM);
  ST("Z 2") q += PrintParameter(q, 'O', rollOffM);
  ST(" Y2") q += PrintParameter(q, 'P', streamIdM);
  ST(" Y2") q += PrintParameter(q, 'Q', t2SystemIdM);
  ST("ZY*") q += PrintParameter(q, 'S', systemM);
  ST(" Y*") q += PrintParameter(q, 'T', transmissionM);
  ST(" Y2") q += PrintParameter(q, 'X', sisoMisoM);
  ST("Z *") q += PrintParameter(q, 'Z', signalSourceM);
#undef ST
  return buffer;
}

const char *cSatipTransponderParameters::ParseParameter(const char *strP, int &valueP)
{
  if (*++strP) {
     char *p = NULL;
     errno = 0;
     int n = strtol(strP, &p, 10);
     if (!errno && p != strP) {
        valueP = n;
        if (valueP >= 0)
           return p;
        }
     }
  error("invalid value for parameter '%c'", *(strP - 1));
  return NULL;
}

bool cSatipTransponderParameters::Parse(const char *strP)
{
  while (strP && *strP) {
        int ignoreThis;
        switch (toupper(*strP)) {
          case 'B': strP = ParseParameter(strP, bandwidthM);    break;
          case 'C': strP = ParseParameter(strP, coderateHM);    break;
          case 'G': strP = ParseParameter(strP, guardM);        break;
          case 'H': polarizationM = 'H'; strP++;                break;
          case 'L': polarizationM = 'L'; strP++;                break;
          case 'M': strP = ParseParameter(strP, modulationM);   break;
          case 'N': strP = ParseParameter(strP, pilotTonesM);   break;
          case 'O': strP = ParseParameter(strP, rollOffM);      break;
          case 'P': strP = ParseParameter(strP, streamIdM);     break;
          case 'Q': strP = ParseParameter(strP, t2SystemIdM);   break;
          case 'R': polarizationM = 'R'; strP++;                break;
          case 'S': strP = ParseParameter(strP, systemM);       break;
          case 'T': strP = ParseParameter(strP, transmissionM); break;
          case 'V': polarizationM = 'V'; strP++;                break;
          case 'X': strP = ParseParameter(strP, sisoMisoM);     break;
          case 'Z': strP = ParseParameter(strP, signalSourceM); break;
          case 'D': strP = ParseParameter(strP, ignoreThis);    break; /* silently ignore coderate low priority */
          case 'I': strP = ParseParameter(strP, ignoreThis);    break; /* silently ignore inversion */
          case 'Y': strP = ParseParameter(strP, ignoreThis);    break; /* silently ignore hierarchy */
          default: esyslog("ERROR: unknown parameter key '%c'", *strP);
                   return false;
          }
        }
  return true;
}

// --- cSatipSourceParam ------------------------------------------------------

cSatipSourceParam::cSatipSourceParam(char sourceP, const char *descriptionP)
: cSourceParam(sourceP, descriptionP),
  paramM(0),
  nidM(0),
  tidM(0),
  ridM(0),
  srateM(0),
  dataM(),
  stpM()
{
  debug("cSatipSourceParam::%s(%c, %s)", __FUNCTION__, sourceP, descriptionP);
}

void cSatipSourceParam::SetData(cChannel *channelP)
{
  debug("cSatipSourceParam::%s(%s)", __FUNCTION__, channelP->Parameters());
  dataM = *channelP;
  nidM = dataM.Nid();
  tidM = dataM.Tid();
  ridM = dataM.Rid();
  srateM = dataM.Srate();
  stpM.Parse(dataM.Parameters());
  paramM = 0;
}

void cSatipSourceParam::GetData(cChannel *channelP)
{
  debug("cSatipSourceParam::%s(%s)", __FUNCTION__, channelP->Parameters());
  channelP->SetTransponderData(channelP->Source(), channelP->Frequency(), srateM, stpM.ToString(Source()), true);
  channelP->SetId(nidM, tidM, channelP->Sid(), ridM);
}

cOsdItem *cSatipSourceParam::GetOsdItem(void)
{
  char type = Source();
  const tSatipParameterMap *SatipSystemValues = type == 'Z' ? SatipSystemValuesSat : SatipSystemValuesTerr;
#define ST(s) if (strchr(s, type))
  switch (paramM++) {
    case  0:           return new cMenuEditIntItem(      tr("Nid"),         &nidM, 0);
    case  1:           return new cMenuEditIntItem(      tr("Tid"),         &tidM, 0);
    case  2:           return new cMenuEditIntItem(      tr("Rid"),         &ridM, 0);
    case  3: ST("Z ")  return new cMenuEditIntItem(  trVDR("Srate"),        &srateM);                                      else return GetOsdItem();
    case  4: ST("Z ")  return new cMenuEditIntItem(     tr("SignalSource"), &stpM.signalSourceM, 1, 255);                  else return GetOsdItem();
    case  5: ST("Z ")  return new cMenuEditChrItem(  trVDR("Polarization"), &stpM.polarizationM, "HVLR");                  else return GetOsdItem();
    case  6: ST("Z ")  return new cMenuEditSatipItem(trVDR("Rolloff"),      &stpM.rollOffM,      SatipRollOffValues);      else return GetOsdItem();
    case  7: ST("Z ")  return new cMenuEditSatipItem(   tr("PilotTones"),   &stpM.pilotTonesM,   SatipPilotTonesValues);   else return GetOsdItem();
    case  8: ST("ZY")  return new cMenuEditSatipItem(trVDR("System"),       &stpM.systemM,       SatipSystemValues);       else return GetOsdItem();
    case  9: ST("ZY")  return new cMenuEditSatipItem(trVDR("Modulation"),   &stpM.modulationM,   SatipModulationValues);   else return GetOsdItem();
    case 10: ST("ZY")  return new cMenuEditSatipItem(trVDR("CoderateH"),    &stpM.coderateHM,    SatipCodeRateValues);     else return GetOsdItem();
    case 11: ST(" Y")  return new cMenuEditSatipItem(trVDR("Bandwidth"),    &stpM.bandwidthM,    SatipBandwidthValues);    else return GetOsdItem();
    case 12: ST(" Y")  return new cMenuEditSatipItem(trVDR("Transmission"), &stpM.transmissionM, SatipTransmissionValues); else return GetOsdItem();
    case 13: ST(" Y")  return new cMenuEditSatipItem(trVDR("Guard"),        &stpM.guardM,        SatipGuardValues);        else return GetOsdItem();
    case 14: ST(" Y")  return new cMenuEditIntItem(  trVDR("StreamId"),     &stpM.streamIdM,     0, 255);                  else return GetOsdItem();
    case 15: ST(" Y")  return new cMenuEditIntItem(     tr("T2SystemId"),   &stpM.t2SystemIdM,   0, 65535);                else return GetOsdItem();
    case 16: ST(" Y")  return new cMenuEditSatipItem(   tr("SISO/MISO"),    &stpM.sisoMisoM,     SatipSisoMisoValues);     else return GetOsdItem();
    default: return NULL;
    }
#undef ST
  return NULL;
}
