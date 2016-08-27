/*
 * param.h: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __SATIP_PARAM_H
#define __SATIP_PARAM_H

#include "common.h"

cString GetTransponderUrlParameters(const cChannel *channelP);
cString GetTnrUrlParameters(const cChannel *channelP);

#endif // __SATIP_PARAM_H
