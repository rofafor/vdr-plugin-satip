/*
 * log.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_LOG_H
#define __SATIP_LOG_H

#include "config.h"

#define error(x...)   esyslog("SATIP-ERROR: " x)
#define info(x...)    isyslog("SATIP: " x)
// 0x0001: Generic call stack
#define debug1(x...)  void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug1)  ? dsyslog("SATIP1: " x)  : void() )
// 0x0002: CURL data flow
#define debug2(x...)  void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug2)  ? dsyslog("SATIP2: " x)  : void() )
// 0x0004: Data parsing
#define debug3(x...)  void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug3)  ? dsyslog("SATIP3: " x)  : void() )
// 0x0008: Tuner state machine
#define debug4(x...)  void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug4)  ? dsyslog("SATIP4: " x)  : void() )
// 0x0010: RTSP responses
#define debug5(x...)  void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug5)  ? dsyslog("SATIP5: " x)  : void() )
// 0x0020: RTP throughput performance
#define debug6(x...)  void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug6)  ? dsyslog("SATIP6: " x)  : void() )
// 0x0040: RTP packet internals
#define debug7(x...)  void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug7)  ? dsyslog("SATIP7: " x)  : void() )
// 0x0080: Section filtering
#define debug8(x...)  void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug8)  ? dsyslog("SATIP8: " x)  : void() )
// 0x0100: Channel switching
#define debug9(x...)  void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug9)  ? dsyslog("SATIP9: " x)  : void() )
// 0x0200: RTCP packets
#define debug10(x...) void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug10) ? dsyslog("SATIP10: " x) : void() )
// 0x0400: TBD
#define debug11(x...) void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug11) ? dsyslog("SATIP11: " x) : void() )
// 0x0800: TBD
#define debug12(x...) void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug12) ? dsyslog("SATIP12: " x) : void() )
// 0x1000: TBD
#define debug13(x...) void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug13) ? dsyslog("SATIP13: " x) : void() )
// 0x2000: TBD
#define debug14(x...) void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug14) ? dsyslog("SATIP14: " x) : void() )
// 0x4000: TBD
#define debug15(x...) void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug15) ? dsyslog("SATIP15: " x) : void() )
// 0x8000; Extra call stack
#define debug16(x...) void( SatipConfig.IsTraceMode(cSatipConfig::eTraceModeDebug16) ? dsyslog("SATIP16: " x) : void() )

#endif // __SATIP_LOG_H

