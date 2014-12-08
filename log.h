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
#define debug1(x...)  void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug1)  ? dsyslog("SATIP1: " x)  : void() )
#define debug2(x...)  void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug2)  ? dsyslog("SATIP2: " x)  : void() )
#define debug3(x...)  void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug3)  ? dsyslog("SATIP3: " x)  : void() )
#define debug4(x...)  void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug4)  ? dsyslog("SATIP4: " x)  : void() )
#define debug5(x...)  void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug5)  ? dsyslog("SATIP5: " x)  : void() )
#define debug6(x...)  void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug6)  ? dsyslog("SATIP6: " x)  : void() )
#define debug7(x...)  void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug7)  ? dsyslog("SATIP7: " x)  : void() )
#define debug8(x...)  void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug8)  ? dsyslog("SATIP8: " x)  : void() )
#define debug9(x...)  void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug9)  ? dsyslog("SATIP9: " x)  : void() )
#define debug10(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug10) ? dsyslog("SATIP10: " x) : void() )
#define debug11(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug11) ? dsyslog("SATIP11: " x) : void() )
#define debug12(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug12) ? dsyslog("SATIP12: " x) : void() )
#define debug13(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug13) ? dsyslog("SATIP13: " x) : void() )
#define debug14(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug14) ? dsyslog("SATIP14: " x) : void() )
#define debug15(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug15) ? dsyslog("SATIP15: " x) : void() )
#define debug16(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug16) ? dsyslog("SATIP16: " x) : void() )

#endif // __SATIP_LOG_H

