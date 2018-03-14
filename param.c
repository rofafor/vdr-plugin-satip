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
  {  5000000, "&bw=5"     },
  {  6000000, "&bw=6"     },
  {  7000000, "&bw=7"     },
  {  8000000, "&bw=8"     },
  { 10000000, "&bw=10"    },
  {  1712000, "&bw=1.712" },
  {       -1, NULL        }
};

static const tSatipParameterMap SatipPilotValues[] = {
  {  PILOT_OFF, "&plts=off" },
  {   PILOT_ON, "&plts=on"  },
  { PILOT_AUTO, ""          },
  {         -1, NULL        }
};

static const tSatipParameterMap SatipSisoMisoValues[] = {
  {   0, "&sm=0" },
  {   1, "&sm=1" },
  {  -1, NULL    }
};

static const tSatipParameterMap SatipCodeRateValues[] = {
  { FEC_NONE, ""         },
  {  FEC_1_2, "&fec=12"  },
  {  FEC_2_3, "&fec=23"  },
  {  FEC_3_4, "&fec=34"  },
  {  FEC_3_5, "&fec=35"  },
  {  FEC_4_5, "&fec=45"  },
  {  FEC_5_6, "&fec=56"  },
  {  FEC_6_7, "&fec=67"  },
  {  FEC_7_8, "&fec=78"  },
  {  FEC_8_9, "&fec=89"  },
  { FEC_9_10, "&fec=910" },
  { FEC_AUTO, ""         },
  {       -1, NULL       }
};

static const tSatipParameterMap SatipModulationValues[] = {
  {     QPSK, "&mtype=qpsk"   },
  {    PSK_8, "&mtype=8psk"   },
  {   QAM_16, "&mtype=16qam"  },
  {   QAM_64, "&mtype=64qam"  },
  {  QAM_128, "&mtype=128qam" },
  {  QAM_256, "&mtype=256qam" },
  { QAM_AUTO, ""              },
  {       -1, NULL            }
};

static const tSatipParameterMap SatipSystemValuesSat[] = {
  {   0, "&msys=dvbs"  },
  {   1, "&msys=dvbs2" },
  {  -1, NULL          }
};

static const tSatipParameterMap SatipSystemValuesTerrestrial[] = {
  {   0, "&msys=dvbt"  },
  {   1, "&msys=dvbt2" },
  {  -1, NULL          }
};

static const tSatipParameterMap SatipSystemValuesCable[] = {
  {   0, "&msys=dvbc"  },
  {   1, "&msys=dvbc2" },
  {  -1, NULL          }
};

static const tSatipParameterMap SatipTransmissionValues[] = {
  {   TRANSMISSION_MODE_1K, "&tmode=1k"  },
  {   TRANSMISSION_MODE_2K, "&tmode=2k"  },
  {   TRANSMISSION_MODE_4K, "&tmode=4k"  },
  {   TRANSMISSION_MODE_8K, "&tmode=8k"  },
  {  TRANSMISSION_MODE_16K, "&tmode=16k" },
  {  TRANSMISSION_MODE_32K, "&tmode=32k" },
  { TRANSMISSION_MODE_AUTO, ""           },
  {                     -1, NULL         }
};

static const tSatipParameterMap SatipGuardValues[] = {
  {    GUARD_INTERVAL_1_4, "&gi=14"    },
  {    GUARD_INTERVAL_1_8, "&gi=18"    },
  {   GUARD_INTERVAL_1_16, "&gi=116"   },
  {   GUARD_INTERVAL_1_32, "&gi=132"   },
  {  GUARD_INTERVAL_1_128, "&gi=1128"  },
  { GUARD_INTERVAL_19_128, "&gi=19128" },
  { GUARD_INTERVAL_19_256, "&gi=19256" },
  {   GUARD_INTERVAL_AUTO, ""          },
  {                    -1, NULL        }
};

static const tSatipParameterMap SatipRollOffValues[] = {
  { ROLLOFF_AUTO, ""         },
  {   ROLLOFF_20, "&ro=0.20" },
  {   ROLLOFF_25, "&ro=0.25" },
  {   ROLLOFF_35, "&ro=0.35" },
  {           -1, NULL       }
};

static const tSatipParameterMap SatipInversionValues[] = {
  { INVERSION_AUTO, ""           },
  {  INVERSION_OFF, "&specinv=0" },
  {   INVERSION_ON, "&specinv=1" },
  {             -1, NULL         }
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

static int PrintUrlString(char *bufP, int lenP, int valueP, const tSatipParameterMap *mapP)
{
  int n = SatipUserIndex(valueP, mapP);
  return ((n >= 0) && (lenP > 0)) ? snprintf(bufP, lenP, "%s", mapP[n].satipString) : 0;
}

cString GetTransponderUrlParameters(const cChannel *channelP)
{
  if (channelP) {
     char buffer[255];
     cDvbTransponderParameters dtp(channelP->Parameters());
     int DataSlice = 0;
     int C2TuningFrequencyType = 0;
     float freq = channelP->Frequency();
     char type = cSource::ToChar(channelP->Source());
     cSource *source = Sources.Get(channelP->Source());
     int src = (strchr("S", type) && source) ? atoi(source->Description()) : 1;
     char *q = buffer;
     *q = 0;
     // Scale down frequencies to MHz
     while (freq > 20000L)
           freq /= 1000L;
#define ST(s) if (strchr(s, type) && (strchr(s, '0' + dtp.System() + 1) || strchr(s, '*')))
#define STBUFLEFT (sizeof(buffer) - (q - buffer))
     ST(" S 1") { // to comply with SAT>IP protocol specification 1.2.2
       dtp.SetPilot(PILOT_OFF);
       dtp.SetModulation(QPSK);
       dtp.SetRollOff(ROLLOFF_35);
       }
     if ((channelP->Rid() % 100) > 0)
                q += snprintf(q,       STBUFLEFT, "&fe=%d",           channelP->Rid() % 100);
     ST(" S *") q += snprintf(q,       STBUFLEFT, "src=%d&",          ((src > 0) && (src <= 255)) ? src : 1);
     if (freq >= 0L)
                q += snprintf(q,       STBUFLEFT, "freq=%s",          *dtoa(freq, "%lg"));
     ST(" S *") q += snprintf(q,       STBUFLEFT, "&pol=%c",          tolower(dtp.Polarization()));
     ST(" S *") q += PrintUrlString(q, STBUFLEFT, dtp.RollOff(),      SatipRollOffValues);
     ST("C  2") q += snprintf(q,       STBUFLEFT, "&c2tft=%d",        C2TuningFrequencyType);
     ST("  T*") q += PrintUrlString(q, STBUFLEFT, dtp.Bandwidth(),    SatipBandwidthValues);
     ST("C  2") q += PrintUrlString(q, STBUFLEFT, dtp.Bandwidth(),    SatipBandwidthValues);
     ST(" S *") q += PrintUrlString(q, STBUFLEFT, dtp.System(),       SatipSystemValuesSat);
     ST("C  *") q += PrintUrlString(q, STBUFLEFT, dtp.System(),       SatipSystemValuesCable);
     ST("  T*") q += PrintUrlString(q, STBUFLEFT, dtp.System(),       SatipSystemValuesTerrestrial);
     ST("  T*") q += PrintUrlString(q, STBUFLEFT, dtp.Transmission(), SatipTransmissionValues);
     ST(" S *") q += PrintUrlString(q, STBUFLEFT, dtp.Modulation(),   SatipModulationValues);
     ST("  T*") q += PrintUrlString(q, STBUFLEFT, dtp.Modulation(),   SatipModulationValues);
     ST("C  1") q += PrintUrlString(q, STBUFLEFT, dtp.Modulation(),   SatipModulationValues);
     ST(" S *") q += PrintUrlString(q, STBUFLEFT, dtp.Pilot(),        SatipPilotValues);
     ST(" S *") q += snprintf(q,       STBUFLEFT, "&sr=%d",           channelP->Srate());
     ST("C  1") q += snprintf(q,       STBUFLEFT, "&sr=%d",           channelP->Srate());
     ST("  T*") q += PrintUrlString(q, STBUFLEFT, dtp.Guard(),        SatipGuardValues);
     ST("CST*") q += PrintUrlString(q, STBUFLEFT, dtp.CoderateH(),    SatipCodeRateValues);
     ST("C  2") q += snprintf(q,       STBUFLEFT, "&ds=%d",           DataSlice);
     ST("C T2") q += snprintf(q,       STBUFLEFT, "&plp=%d",          dtp.StreamId());
     ST("  T2") q += snprintf(q,       STBUFLEFT, "&t2id=%d",         dtp.T2SystemId());
     ST("  T2") q += PrintUrlString(q, STBUFLEFT, dtp.SisoMiso(),     SatipSisoMisoValues);
     ST("C  1") q += PrintUrlString(q, STBUFLEFT, dtp.Inversion(),    SatipInversionValues);
#undef ST
     return buffer;
     }
  return NULL;
}

cString GetTnrUrlParameters(const cChannel *channelP)
{
  if (channelP) {
     cDvbTransponderParameters dtp(channelP->Parameters());
     eTrackType track = cDevice::PrimaryDevice()->GetCurrentAudioTrack();

     // TunerType: Byte;
     //   0 = cable, 1 = satellite, 2 = terrestrial, 3 = atsc, 4 = iptv, 5 = stream (URL, DVBViewer GE)
     int TunerType = 0;
     if (channelP->IsCable())
        TunerType = 0;
     else if (channelP->IsSat())
        TunerType = 1;
     else if (channelP->IsTerr())
        TunerType = 2;
     else if (channelP->IsAtsc())
        TunerType = 3;

     // Frequency: DWord;
     //   DVB-S: MHz if < 1000000, kHz if >= 1000000
     //   DVB-T/C, ATSC: kHz
     //   IPTV: IP address Byte3.Byte2.Byte1.Byte0
     int Frequency = channelP->Frequency() / 1000;

     // Symbolrate: DWord;
     //   DVB S/C: in kSym/s
     //   DVB-T, ATSC: 0
     //   IPTV: Port
     int Symbolrate = (channelP->IsSat() || channelP->IsCable()) ? channelP->Srate() : 0;

     // LNB_LOF: Word;
     //   DVB-S: Local oscillator frequency of the LNB
     //   DVB-T/C, ATSC: 0
     //   IPTV: Byte0 and Byte1 of Source IP
     int LNB_LOF = channelP->IsSat() ? Setup.LnbSLOF : 0;

     // Tone: Byte;
     //   0 = off, 1 = 22 khz
     int Tone = (channelP->Frequency() < Setup.LnbSLOF) ? 0 : 1;

     // Polarity: Byte;
     //   DVB-S polarity: 0 = horizontal, 1 = vertical, 2 = circular left, 3 = circular right
     //   DVB-C modulation: 0 = Auto, 1 = 16QAM, 2 = 32QAM, 3 = 64QAM, 4 = 128QAM, 5 = 256 QAM
     //   DVB-T bandwidth: 0 = 6 MHz, 1 = 7 MHz, 2 = 8 MHz
     //   IPTV: Byte3 of SourceIP
     int Polarity = 0;
     if (channelP->IsSat()) {
        switch (tolower(dtp.Polarization())) {
          case 'h':
               Polarity = 0;
               break;
          case 'v':
               Polarity = 1;
               break;
          case 'l':
               Polarity = 2;
               break;
          case 'r':
               Polarity = 3;
               break;
          default:
               break;
          }
        }
     else if (channelP->IsCable()) {
        switch (dtp.Modulation()) {
          case 999:
               Polarity = 0;
               break;
          case 16:
               Polarity = 1;
               break;
          case 32:
               Polarity = 2;
               break;
          case 64:
               Polarity = 3;
               break;
          case 128:
               Polarity = 4;
               break;
          case 256:
               Polarity = 5;
               break;
          default:
               break;
          }
        }
     else if (channelP->IsTerr()) {
        switch (dtp.Bandwidth()) {
          case 6:
               Polarity = 0;
               break;
          case 7:
               Polarity = 1;
               break;
          case 8:
               Polarity = 2;
               break;
          default:
               break;
          }
        }

     // DiSEqC: Byte;
     //   0 = None
     //   1 = Pos A (mostly translated to PosA/OptA)
     //   2 = Pos B (mostly translated to PosB/OptA)
     //   3 = PosA/OptA
     //   4 = PosB/OptA
     //   5 = PosA/OptB
     //   6 = PosB/OptB
     //   7 = Preset Position (DiSEqC 1.2, see DiSEqCExt)
     //   8 = Angular Position (DiSEqC 1.2, see DiSEqCExt)
     //   9 = DiSEqC Command Sequence (see DiSEqCExt)
     int DiSEqC = 0;

     // FEC: Byte;
     //   0 = Auto
     //   1 = 1/2
     //   2 = 2/3
     //   3 = 3/4
     //   4 = 5/6
     //   5 = 7/8
     //   6 = 8/9
     //   7 = 3/5
     //   8 = 4/5
     //   9 = 9/10
     //   IPTV: Byte2 of SourceIP
     //   DVB C/T, ATSC: 0
     int FEC = 0;
     if (channelP->IsSat()) {
        switch (dtp.CoderateH()) {
          case 999:
               FEC = 0;
               break;
          case 12:
               FEC = 1;
               break;
          case 23:
               FEC = 2;
               break;
          case 34:
               FEC = 3;
               break;
          case 56:
               FEC = 4;
               break;
          case 78:
               FEC = 5;
               break;
          case 89:
               FEC = 6;
               break;
          case 35:
               FEC = 7;
               break;
          case 45:
               FEC = 8;
               break;
          case 910:
               FEC = 9;
               break;
          default:
               break;
          }
        }

     // Audio_PID: Word;
     int Audio_PID = channelP->Apid(0);
     if (IS_AUDIO_TRACK(track))
        Audio_PID = channelP->Apid(int(track - ttAudioFirst));
     else if (IS_DOLBY_TRACK(track))
        Audio_PID = channelP->Dpid(int(track - ttDolbyFirst));

     // Video_PID: Word;
     int Video_PID = channelP->Vpid();

     // PMT_PID: Word;
     int PMT_PID = channelP->Ppid();

     // Service_ID: Word;
     int Service_ID = channelP->Sid();

     // SatModulation: Byte;
     //   Bit 0..1: satellite modulation. 0 = Auto, 1 = QPSK, 2 = 8PSK, 3 = 16QAM or APSK for DVB-S2
     //   Bit 2: modulation system. 0 = DVB-S/T/C, 1 = DVB-S2/T2/C2
     //   Bit 3..4: DVB-S2: roll-off. 0 = 0.35, 1 = 0.25, 2 = 0.20, 3 = reserved
     //   Bit 5..6: spectral inversion, 0 = undefined, 1 = auto, 2 = normal, 3 = inverted
     //   Bit 7: DVB-S2: pilot symbols, 0 = off, 1 = on
     //          DVB-T2: DVB-T2 Lite, 0 = off, 1 = on
     int SatModulation = 0;
     if (channelP->IsSat() && dtp.System()) {
        switch (dtp.Modulation()) {
          case 999:
               SatModulation |= (0 & 0x3) << 0;
               break;
          case 2:
               SatModulation |= (1 & 0x3) << 0;
               break;
          case 5:
               SatModulation |= (2 & 0x3) << 0;
               break;
          case 6:
               SatModulation |= (3 & 0x3) << 0;
               break;
          default:
               break;
          }
        }
     SatModulation |= (dtp.System() & 0x1) << 2;
     if (channelP->IsSat() && dtp.System()) {
        switch (dtp.RollOff()) {
          case 35:
               SatModulation |= (0 & 0x3) << 3;
               break;
          case 25:
               SatModulation |= (1 & 0x3) << 3;
               break;
          case 20:
               SatModulation |= (2 & 0x3) << 3;
               break;
          default:
               break;
          }
        }
     switch (dtp.Inversion()) {
       case 999:
           SatModulation |= (1 & 0x3) << 5;
           break;
       case 0:
           SatModulation |= (2 & 0x3) << 5;
           break;
       case 1:
           SatModulation |= (3 & 0x3) << 5;
           break;
       default:
           break;
       }
     if (channelP->IsSat() && dtp.System()) {
        switch (dtp.Pilot()) {
          case 0:
               SatModulation |= (0 & 0x1) << 7;
               break;
          case 1:
               SatModulation |= (1 & 0x1) << 7;
               break;
          default:
               break;
          }
        }

     // DiSEqCExt: Word;
     //   DiSEqC Extension, meaning depends on DiSEqC
     //   DiSEqC = 0..6: 0
     //   DiSEqC = 7: Preset Position (DiSEqC 1.2)
     //   DiSEqC = 8: Orbital Position (DiSEqC 1.2, USALS, for calculating motor angle)
     //               Same format as OrbitalPos above
     //   DiSEQC = 9: Orbital Position referencing DiSEqC sequence defined in DiSEqC.xml/ini
     //               Same format as OrbitalPos above
     int DiSEqCExt = 0;

     // Flags: Byte;
     //   Bit 0: 1 = encrypted channel
     //   Bit 1: reserved, set to 0
     //   Bit 2: 1 = channel broadcasts RDS data
     //   Bit 3: 1 = channel is a video service (even if the Video PID is temporarily = 0)
     //   Bit 4: 1 = channel is an audio service (even if the Audio PID is temporarily = 0)
     //   Bit 5: 1 = audio has a different samplerate than 48 KHz
     //   Bit 6: 1 = bandstacking, internally polarisation is always set to H
     //   Bit 7: 1 = channel entry is an additional audio track of the preceding
     //              channel with bit 7 = 0
     int Flags = (channelP->Ca() > 0xFF) ? 1 : 0;

     // ChannelGroup: Byte;
     //   0 = Group A, 1 = Group B, 2 = Group C etc.
     int ChannelGroup = 0;

     // TransportStream_ID: Word;
     int TransportStream_ID = channelP->Tid();

     // OriginalNetwork_ID: Word;
     int OriginalNetwork_ID = channelP->Nid();

     // Substream: Word;
     //   DVB-S/C/T, ATSC, IPTV: 0
     //   DVB-T2: 0 = PLP_ID not set, 1..256: PLP_ID + 1, 257... reserved
     int Substream = (channelP->IsTerr() && dtp.System()) ? dtp.StreamId() - 1 : 0;

     // OrbitalPos: Word;
     //   DVB-S: orbital position x 10, 0 = undefined, 1..1800 east, 1801..3599 west (1°W = 3599)
     //   DVB-C: 4000..4999
     //   DVB-T: 5000..5999
     //   ATSC:  6000..6999
     //   IPTV:  7000..7999
     //   Stream: 8000..8999
     int OrbitalPos = 0;
     if (channelP->IsSat()) {
        OrbitalPos = cSource::Position(channelP->Source());
        if (OrbitalPos != 3600)
           OrbitalPos += 1800;
        }

     return cString::sprintf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                             TunerType, Frequency, Symbolrate, LNB_LOF, Tone, Polarity, DiSEqC, FEC, Audio_PID, Video_PID, PMT_PID, Service_ID,
                             SatModulation, DiSEqCExt, Flags, ChannelGroup, TransportStream_ID, OriginalNetwork_ID, Substream, OrbitalPos);
     }
  return NULL;
}
