/*
 * satip.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <ctype.h>
#include <getopt.h>
#include <vdr/plugin.h>
#include "common.h"
#include "config.h"
#include "device.h"
#include "discover.h"
#include "log.h"
#include "poller.h"
#include "setup.h"

#if defined(LIBCURL_VERSION_NUM) && LIBCURL_VERSION_NUM < 0x072400
#warning "CURL version >= 7.36.0 is recommended"
#endif

#if defined(APIVERSNUM) && APIVERSNUM < 20200
#error "VDR-2.2.0 API version or greater is required!"
#endif

#ifndef GITVERSION
#define GITVERSION ""
#endif

       const char VERSION[]     = "2.2.5" GITVERSION;
static const char DESCRIPTION[] = trNOOP("SAT>IP Devices");

class cPluginSatip : public cPlugin {
private:
  unsigned int deviceCountM;
  cSatipDiscoverServers *serversM;
  void ParseServer(const char *paramP);
  void ParsePortRange(const char *paramP);
  int ParseCicams(const char *valueP, int *cicamsP);
  int ParseSources(const char *valueP, int *sourcesP);
  int ParseFilters(const char *valueP, int *filtersP);
public:
  cPluginSatip(void);
  virtual ~cPluginSatip();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Stop(void);
  virtual void Housekeeping(void);
  virtual void MainThreadHook(void);
  virtual cString Active(void);
  virtual time_t WakeupTime(void);
  virtual const char *MainMenuEntry(void) { return NULL; }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  virtual bool Service(const char *Id, void *Data = NULL);
  virtual const char **SVDRPHelpPages(void);
  virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
  };

cPluginSatip::cPluginSatip(void)
: deviceCountM(2),
  serversM(NULL)
{
  debug16("%s", __PRETTY_FUNCTION__);
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginSatip::~cPluginSatip()
{
  debug16("%s", __PRETTY_FUNCTION__);
  // Clean up after yourself!
}

const char *cPluginSatip::CommandLineHelp(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Return a string that describes all known command line options.
  return "  -d <num>, --devices=<number>  set number of devices to be created\n"
         "  -t <mode>, --trace=<mode>     set the tracing mode\n"
         "  -s <ipaddr>|<model>|<desc>, --server=<ipaddr1>|<model1>|<desc1>;<ipaddr2>:<port>|<model2>:<filter>|<desc2>:<quirk>\n"
         "                                define hard-coded SAT>IP server(s)\n"
         "  -D, --detach                  set the detached mode on\n"
         "  -S, --single                  set the single model server mode on\n"
         "  -n, --noquirks                disable autodetection of the server quirks\n"
         "  -p, --portrange=<start>-<end> set a range of ports used for the RT[C]P server\n"
         "                                a minimum of 2 ports per device is required.\n"
         "  -r, --rcvbuf                  override the size of the RTP receive buffer in bytes\n";
}

bool cPluginSatip::ProcessArgs(int argc, char *argv[])
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Implement command line argument processing here if applicable.
  static const struct option long_options[] = {
    { "devices",  required_argument, NULL, 'd' },
    { "trace",    required_argument, NULL, 't' },
    { "server",   required_argument, NULL, 's' },
    { "portrange",required_argument, NULL, 'p' },
    { "rcvbuf",   required_argument, NULL, 'r' },
    { "detach",   no_argument,       NULL, 'D' },
    { "single",   no_argument,       NULL, 'S' },
    { "noquirks", no_argument,       NULL, 'n' },
    { NULL,       no_argument,       NULL,  0  }
    };

  cString server;
  cString portrange;
  int c;
  while ((c = getopt_long(argc, argv, "d:t:s:p:r:DSn", long_options, NULL)) != -1) {
    switch (c) {
      case 'd':
           deviceCountM = strtol(optarg, NULL, 0);
           break;
      case 't':
           SatipConfig.SetTraceMode(strtol(optarg, NULL, 0));
           break;
      case 's':
           server = optarg;
           break;
      case 'D':
           SatipConfig.SetDetachedMode(true);
           break;
      case 'S':
           SatipConfig.SetUseSingleModelServers(true);
           break;
      case 'n':
           SatipConfig.SetDisableServerQuirks(true);
           break;
      case 'p':
           portrange = optarg;
           break;
      case 'r':
           SatipConfig.SetRtpRcvBufSize(strtol(optarg, NULL, 0));
           break;
      default:
           return false;
      }
    }
  if (!isempty(*portrange))
     ParsePortRange(portrange);
  // this must be done after all parameters are parsed
  if (!isempty(*server))
     ParseServer(*server);
  return true;
}

bool cPluginSatip::Initialize(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Initialize any background activities the plugin shall perform.
  if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
     error("Unable to initialize CURL");
  cSatipPoller::GetInstance()->Initialize();
  cSatipDiscover::GetInstance()->Initialize(serversM);
  return cSatipDevice::Initialize(deviceCountM);
}

bool cPluginSatip::Start(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Start any background activities the plugin shall perform.
  curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
  cString info = cString::sprintf("Using CURL %s", data->version);
  for (int i = 0; data->protocols[i]; ++i) {
      // Supported protocols: HTTP(S), RTSP, FILE
      if (startswith(data->protocols[i], "rtsp"))
         info = cString::sprintf("%s %s", *info, data->protocols[i]);
      }
  info("%s", *info);
  return true;
}

void cPluginSatip::Stop(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Stop any background activities the plugin is performing.
  cSatipDevice::Shutdown();
  cSatipDiscover::GetInstance()->Destroy();
  cSatipPoller::GetInstance()->Destroy();
  curl_global_cleanup();
}

void cPluginSatip::Housekeeping(void)
{
  debug16("%s", __PRETTY_FUNCTION__);
  // Perform any cleanup or other regular tasks.
}

void cPluginSatip::MainThreadHook(void)
{
  debug16("%s", __PRETTY_FUNCTION__);
  // Perform actions in the context of the main program thread.
  // WARNING: Use with great care - see PLUGINS.html!
}

cString cPluginSatip::Active(void)
{
  debug16("%s", __PRETTY_FUNCTION__);
  // Return a message string if shutdown should be postponed
  return NULL;
}

time_t cPluginSatip::WakeupTime(void)
{
  debug16("%s", __PRETTY_FUNCTION__);
  // Return custom wakeup time for shutdown script
  return 0;
}

cOsdObject *cPluginSatip::MainMenuAction(void)
{
  debug16("%s", __PRETTY_FUNCTION__);
  // Perform the action when selected from the main VDR menu.
  return NULL;
}

cMenuSetupPage *cPluginSatip::SetupMenu(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Return a setup menu in case the plugin supports one.
  return new cSatipPluginSetup();
}

void cPluginSatip::ParseServer(const char *paramP)
{
  debug1("%s (%s)", __PRETTY_FUNCTION__, paramP);
  int n = 0;
  char *s, *p = strdup(paramP);
  char *r = strtok_r(p, ";", &s);
  while (r) {
        r = skipspace(r);
        debug3("%s server[%d]=%s", __PRETTY_FUNCTION__, n, r);
        cString sourceAddr, serverAddr, serverModel, serverFilters, serverDescription;
        int serverQuirk = cSatipServer::eSatipQuirkNone;
        int serverPort = SATIP_DEFAULT_RTSP_PORT;
        int n2 = 0;
        char *s2, *p2 = r;
        char *r2 = strtok_r(p2, "|", &s2);
        while (r2) {
              debug3("%s param[%d]=%s", __PRETTY_FUNCTION__, n2, r2);
              switch (n2++) {
                     case 0:
                          {
                          char *r3 = strchr(r2, '@');
                          if (r3) {
                             *r3 = 0;
                             sourceAddr = r2;
                             r2 = r3 + 1;
                             }
                          serverAddr = r2;
                          r3 = strchr(r2, ':');
                          if (r3) {
                             serverPort = strtol(r3 + 1, NULL, 0);
                             serverAddr = serverAddr.Truncate(r3 - r2);
                             }
                          }
                          break;
                     case 1:
                          {
                          serverModel = r2;
                          char *r3 = strchr(r2, ':');
                          if (r3) {
                             serverFilters = r3 + 1;
                             serverModel = serverModel.Truncate(r3 - r2);
                             }
                          }
                          break;
                     case 2:
                          {
                          serverDescription = r2;
                          char *r3 = strchr(r2, ':');
                          if (r3) {
                             serverQuirk = strtol(r3 + 1, NULL, 0);
                             serverDescription = serverDescription.Truncate(r3 - r2);
                             }
                          }
                          break;
                     default:
                          break;
                     }
              r2 = strtok_r(NULL, "|", &s2);
              }
        if (*serverAddr && *serverModel && *serverDescription) {
           debug1("%s srcaddr=%s ipaddr=%s port=%d model=%s (%s) desc=%s (%d)", __PRETTY_FUNCTION__, *sourceAddr, *serverAddr, serverPort, *serverModel, *serverFilters, *serverDescription, serverQuirk);
           if (!serversM)
              serversM = new cSatipDiscoverServers();
           serversM->Add(new cSatipDiscoverServer(*sourceAddr, *serverAddr, serverPort, *serverModel, *serverFilters, *serverDescription, serverQuirk));
           }
        ++n;
        r = strtok_r(NULL, ";", &s);
        }
  FREE_POINTER(p);
}

void cPluginSatip::ParsePortRange(const char *paramP)
{
  char *s, *p = skipspace(paramP);
  char *r = strtok_r(p, "-", &s);
  unsigned int rangeStart = 0;
  unsigned int rangeStop = 0;
  if (r) {
     rangeStart = strtol(r, NULL, 0);
     r = strtok_r(NULL, "-", &s);
     }
  if (r)
     rangeStop = strtol(r, NULL, 0);
  else {
     error("Port range argument not valid '%s'", paramP);
     rangeStart = 0;
     rangeStop = 0;
     }
  if (rangeStart % 2) {
     error("The given range start port must be even!");
     rangeStart = 0;
     rangeStop = 0;
     }
  else if (rangeStop - rangeStart + 1 < deviceCountM * 2) {
     error("The given port range is to small: %d < %d!", rangeStop - rangeStart + 1, deviceCountM * 2);
     rangeStart = 0;
     rangeStop = 0;
     }
  SatipConfig.SetPortRangeStart(rangeStart);
  SatipConfig.SetPortRangeStop(rangeStop);
}

int cPluginSatip::ParseCicams(const char *valueP, int *cicamsP)
{
  debug1("%s (%s,)", __PRETTY_FUNCTION__, valueP);
  int n = 0;
  char *s, *p = strdup(valueP);
  char *r = strtok_r(p, " ", &s);
  while (r) {
        r = skipspace(r);
        debug3("%s cicams[%d]=%s", __PRETTY_FUNCTION__, n, r);
        if (n < MAX_CICAM_COUNT) {
           cicamsP[n++] = atoi(r);
           }
        r = strtok_r(NULL, " ", &s);
        }
  FREE_POINTER(p);
  return n;
}

int cPluginSatip::ParseSources(const char *valueP, int *sourcesP)
{
  debug1("%s (%s,)", __PRETTY_FUNCTION__, valueP);
  int n = 0;
  char *s, *p = strdup(valueP);
  char *r = strtok_r(p, " ", &s);
  while (r) {
        r = skipspace(r);
        debug3("%s sources[%d]=%s", __PRETTY_FUNCTION__, n, r);
        if (n < MAX_DISABLED_SOURCES_COUNT) {
           sourcesP[n++] = cSource::FromString(r);
           }
        r = strtok_r(NULL, " ", &s);
        }
  FREE_POINTER(p);
  return n;
}

int cPluginSatip::ParseFilters(const char *valueP, int *filtersP)
{
  debug1("%s (%s,)", __PRETTY_FUNCTION__, valueP);
  char buffer[256];
  int n = 0;
  while (valueP && *valueP && (n < SECTION_FILTER_TABLE_SIZE)) {
    strn0cpy(buffer, valueP, sizeof(buffer));
    int i = atoi(buffer);
    debug3("%s filters[%d]=%d", __PRETTY_FUNCTION__, n, i);
    if (i >= 0)
       filtersP[n++] = i;
    if ((valueP = strchr(valueP, ' ')) != NULL)
       valueP++;
    }
  return n;
}

bool cPluginSatip::SetupParse(const char *nameP, const char *valueP)
{
  debug1("%s", __PRETTY_FUNCTION__);
  // Parse your own setup parameters and store their values.
  if (!strcasecmp(nameP, "OperatingMode"))
     SatipConfig.SetOperatingMode(atoi(valueP));
  else if (!strcasecmp(nameP, "EnableCIExtension"))
     SatipConfig.SetCIExtension(atoi(valueP));
  else if (!strcasecmp(nameP, "CICAM")) {
     int Cicams[MAX_CICAM_COUNT];
     for (unsigned int i = 0; i < ELEMENTS(Cicams); ++i)
         Cicams[i] = 0;
     unsigned int CicamsCount = ParseCicams(valueP, Cicams);
     for (unsigned int i = 0; i < CicamsCount; ++i)
         SatipConfig.SetCICAM(i, Cicams[i]);
     }
  else if (!strcasecmp(nameP, "EnableEITScan"))
     SatipConfig.SetEITScan(atoi(valueP));
  else if (!strcasecmp(nameP, "DisabledSources")) {
     int DisabledSources[MAX_DISABLED_SOURCES_COUNT];
     for (unsigned int i = 0; i < ELEMENTS(DisabledSources); ++i)
         DisabledSources[i] = cSource::stNone;
     unsigned int DisabledSourcesCount = ParseSources(valueP, DisabledSources);
     for (unsigned int i = 0; i < DisabledSourcesCount; ++i)
         SatipConfig.SetDisabledSources(i, DisabledSources[i]);
     }
  else if (!strcasecmp(nameP, "DisabledFilters")) {
     int DisabledFilters[SECTION_FILTER_TABLE_SIZE];
     for (unsigned int i = 0; i < ELEMENTS(DisabledFilters); ++i)
         DisabledFilters[i] = -1;
     unsigned int DisabledFiltersCount = ParseFilters(valueP, DisabledFilters);
     for (unsigned int i = 0; i < DisabledFiltersCount; ++i)
         SatipConfig.SetDisabledFilters(i, DisabledFilters[i]);
     }
  else if (!strcasecmp(nameP, "TransportMode"))
     SatipConfig.SetTransportMode(atoi(valueP));
  else
     return false;
  return true;
}

bool cPluginSatip::Service(const char *idP, void *dataP)
{
  debug1("%s", __PRETTY_FUNCTION__);
  return false;
}

const char **cPluginSatip::SVDRPHelpPages(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  static const char *HelpPages[] = {
    "INFO [ <page> ] [ <card index> ]\n"
    "    Prints SAT>IP device information and statistics.\n"
    "    The output can be narrowed using optional \"page\""
    "    option: 1=general 2=pids 3=section filters.\n",
    "MODE\n"
    "    Toggles between bit or byte information mode.\n",
    "LIST\n"
    "    Lists active SAT>IP servers.\n",
    "SCAN\n"
    "    Scans active SAT>IP servers.\n",
    "STAT\n"
    "    Lists status information of SAT>IP devices.\n",
    "CONT\n"
    "    Shows SAT>IP device count.\n",
    "OPER [ off | low | normal | high ]\n"
    "    Gets and(or sets operating mode of SAT>IP devices.\n",
    "ATTA\n"
    "    Attach active SAT>IP servers.\n",
    "DETA\n"
    "    Detachs active SAT>IP servers.\n",
    "TRAC [ <mode> ]\n"
    "    Gets and/or sets used tracing mode.\n",
    NULL
    };
  return HelpPages;
}

cString cPluginSatip::SVDRPCommand(const char *commandP, const char *optionP, int &replyCodeP)
{
  debug1("%s (%s, %s,)", __PRETTY_FUNCTION__, commandP, optionP);
  if (strcasecmp(commandP, "INFO") == 0) {
     int index = cDevice::ActualDevice()->CardIndex();
     int page = SATIP_DEVICE_INFO_ALL;
     char *opt = strdup(optionP);
     char *num = skipspace(opt);
     char *option = num;
     while (*option && !isspace(*option))
           ++option;
     if (*option) {
        *option = 0;
        option = skipspace(++option);
        if (isnumber(option))
           index = atoi(option);
        }
     if (isnumber(num)) {
        page = atoi(num);
        if ((page < SATIP_DEVICE_INFO_ALL) || (page > SATIP_DEVICE_INFO_FILTERS))
           page = SATIP_DEVICE_INFO_ALL;
        }
     free(opt);
     cSatipDevice *device = cSatipDevice::GetSatipDevice(index);
     if (device) {
        return device->GetInformation(page);
        }
     else {
        replyCodeP = 550; // Requested action not taken
        return cString("SATIP information not available!");
        }
     }
  else if (strcasecmp(commandP, "MODE") == 0) {
     unsigned int mode = !SatipConfig.GetUseBytes();
     SatipConfig.SetUseBytes(mode);
     return cString::sprintf("SATIP information mode: %s\n", mode ? "bytes" : "bits");
     }
  else if (strcasecmp(commandP, "LIST") == 0) {
     cString list = cSatipDiscover::GetInstance()->GetServerList();
     if (!isempty(list)) {
        return list;
        }
     else {
        replyCodeP = 550; // Requested action not taken
        return cString("No SATIP servers detected!");
        }
     }
  else if (strcasecmp(commandP, "SCAN") == 0) {
     cSatipDiscover::GetInstance()->TriggerScan();
     return cString("SATIP server scan requested");
     }
  else if (strcasecmp(commandP, "STAT") == 0) {
     return cSatipDevice::GetSatipStatus();
     }
  else if (strcasecmp(commandP, "CONT") == 0) {
     return cString::sprintf("SATIP device count: %u", cSatipDevice::Count());
     }
  else if (strcasecmp(commandP, "OPER") == 0) {
     cString mode;
     unsigned int oper = SatipConfig.GetOperatingMode();
     if (optionP && *optionP) {
        if (strcasecmp(optionP, "off") == 0)
           oper = cSatipConfig::eOperatingModeOff;
        else if (strcasecmp(optionP, "low") == 0)
           oper = cSatipConfig::eOperatingModeLow;
        else if (strcasecmp(optionP, "normal") == 0)
           oper = cSatipConfig::eOperatingModeNormal;
        else if (strcasecmp(optionP, "high") == 0)
           oper = cSatipConfig::eOperatingModeHigh;
        SatipConfig.SetOperatingMode(oper);
     }
     switch (oper) {
       case cSatipConfig::eOperatingModeOff:
            mode = "off";
            break;
       case cSatipConfig::eOperatingModeLow:
            mode = "low";
            break;
       case cSatipConfig::eOperatingModeNormal:
            mode = "normal";
            break;
       case cSatipConfig::eOperatingModeHigh:
            mode = "high";
            break;
       default:
            mode = "unknown";
            break;
       }
     return cString::sprintf("SATIP operating mode: %s\n", *mode);
     }
  else if (strcasecmp(commandP, "ATTA") == 0) {
     SatipConfig.SetDetachedMode(false);
     info("SATIP servers attached");
     return cString("SATIP servers attached");
     }
  else if (strcasecmp(commandP, "DETA") == 0) {
     SatipConfig.SetDetachedMode(true);
     info("SATIP servers detached");
     return cString("SATIP servers detached");
     }
  else if (strcasecmp(commandP, "TRAC") == 0) {
     if (optionP && *optionP)
        SatipConfig.SetTraceMode(strtol(optionP, NULL, 0));
     return cString::sprintf("SATIP tracing mode: 0x%04X\n", SatipConfig.GetTraceMode());
     }

  return NULL;
}

VDRPLUGINCREATOR(cPluginSatip); // Don't touch this!
