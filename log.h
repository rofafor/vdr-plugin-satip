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
#define debug(x...) void( SatipConfig.IsLogLevelDebug() ? dsyslog("SATIP: " x) : void() )
#define extra(x...) void( SatipConfig.IsLogLevelExtra() ? dsyslog("SATIP: " x) : void() )

#endif // __SATIP_LOG_H

