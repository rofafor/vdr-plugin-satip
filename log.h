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
#define debug1(x...) void( SatipConfig.IsLoggingDebug1() ? dsyslog("SATIP: " x) : void() )
#define debug2(x...) void( SatipConfig.IsLoggingDebug2() ? dsyslog("SATIP: " x) : void() )
#define debug3(x...) void( SatipConfig.IsLoggingDebug3() ? dsyslog("SATIP: " x) : void() )
#define debug4(x...) void( SatipConfig.IsLoggingDebug4() ? dsyslog("SATIP: " x) : void() )
#define debug5(x...) void( SatipConfig.IsLoggingDebug5() ? dsyslog("SATIP: " x) : void() )
#define debug6(x...) void( SatipConfig.IsLoggingDebug6() ? dsyslog("SATIP: " x) : void() )
#define debug7(x...) void( SatipConfig.IsLoggingDebug7() ? dsyslog("SATIP: " x) : void() )
#define debug8(x...) void( SatipConfig.IsLoggingDebug8() ? dsyslog("SATIP: " x) : void() )

#endif // __SATIP_LOG_H

