/*
 * log.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_LOG_H
#define __SATIP_LOG_H

#include "config.h"

#define error(x...) esyslog("SATIP-ERROR: " x)
#define info(x...)  isyslog("SATIP: " x)
#define debug1(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug1) ? dsyslog("SATIP1: " x) : void() )
#define debug2(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug2) ? dsyslog("SATIP2: " x) : void() )
#define debug3(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug3) ? dsyslog("SATIP3: " x) : void() )
#define debug4(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug4) ? dsyslog("SATIP4: " x) : void() )
#define debug5(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug5) ? dsyslog("SATIP5: " x) : void() )
#define debug6(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug6) ? dsyslog("SATIP6: " x) : void() )
#define debug7(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug7) ? dsyslog("SATIP7: " x) : void() )
#define debug8(x...) void( SatipConfig.IsLoggingMode(cSatipConfig::eLoggingModeDebug8) ? dsyslog("SATIP8: " x) : void() )

#endif // __SATIP_LOG_H

