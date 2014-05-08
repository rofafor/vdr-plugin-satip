/*
 * satip.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <getopt.h>
#include <vdr/plugin.h>
#include "common.h"
#include "config.h"
#include "device.h"
#include "discover.h"
#include "setup.h"

#if defined(LIBCURL_VERSION_NUM) && LIBCURL_VERSION_NUM < 0x072400
#warning "CURL version >= 0.7.36 is recommended"
#endif

#if defined(APIVERSNUM) && APIVERSNUM < 20000
#error "VDR-2.0.0 API version or greater is required!"
#endif

#ifndef GITVERSION
#define GITVERSION ""
#endif

       const char VERSION[]     = "0.3.2" GITVERSION;
static const char DESCRIPTION[] = trNOOP("SAT>IP Devices");

class cPluginSatip : public cPlugin {
private:
  unsigned int deviceCountM;
  cSatipDiscover *discoverM;
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
: deviceCountM(1),
  discoverM(NULL)
{
  //debug("cPluginSatip::%s()", __FUNCTION__);
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginSatip::~cPluginSatip()
{
  //debug("cPluginSatip::%s()", __FUNCTION__);
  // Clean up after yourself!
}

const char *cPluginSatip::CommandLineHelp(void)
{
  debug("cPluginSatip::%s()", __FUNCTION__);
  // Return a string that describes all known command line options.
  return "  -d <num>, --devices=<number> number of devices to be created\n";
}

bool cPluginSatip::ProcessArgs(int argc, char *argv[])
{
  debug("cPluginSatip::%s()", __FUNCTION__);
  // Implement command line argument processing here if applicable.
  static const struct option long_options[] = {
    { "devices", required_argument, NULL, 'd' },
    { NULL,      no_argument,       NULL,  0  }
    };

  int c;
  while ((c = getopt_long(argc, argv, "d:", long_options, NULL)) != -1) {
    switch (c) {
      case 'd':
           deviceCountM = atoi(optarg);
           break;
      default:
           return false;
      }
    }
  return true;
}

bool cPluginSatip::Initialize(void)
{
  debug("cPluginSatip::%s()", __FUNCTION__);
  // Initialize any background activities the plugin shall perform.
  if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
     error("Unable to initialize CURL");
  SatipConfig.SetConfigDirectory(cPlugin::ResourceDirectory(PLUGIN_NAME_I18N));
  cSatipDiscover::GetInstance()->Initialize();
  return cSatipDevice::Initialize(deviceCountM);
}

bool cPluginSatip::Start(void)
{
  debug("cPluginSatip::%s()", __FUNCTION__);
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
  debug("cPluginSatip::%s()", __FUNCTION__);
  // Stop any background activities the plugin is performing.
  cSatipDevice::Shutdown();
  cSatipDiscover::GetInstance()->Destroy();
  curl_global_cleanup();
}

void cPluginSatip::Housekeeping(void)
{
  //debug("cPluginSatip::%s()", __FUNCTION__);
  // Perform any cleanup or other regular tasks.
}

void cPluginSatip::MainThreadHook(void)
{
  //debug("cPluginSatip::%s()", __FUNCTION__);
  // Perform actions in the context of the main program thread.
  // WARNING: Use with great care - see PLUGINS.html!
}

cString cPluginSatip::Active(void)
{
  //debug("cPluginSatip::%s()", __FUNCTION__);
  // Return a message string if shutdown should be postponed
  return NULL;
}

time_t cPluginSatip::WakeupTime(void)
{
  //debug("cPluginSatip::%s()", __FUNCTION__);
  // Return custom wakeup time for shutdown script
  return 0;
}

cOsdObject *cPluginSatip::MainMenuAction(void)
{
  //debug("cPluginSatip::%s()", __FUNCTION__);
  // Perform the action when selected from the main VDR menu.
  return NULL;
}

cMenuSetupPage *cPluginSatip::SetupMenu(void)
{
  debug("cPluginSatip::%s()", __FUNCTION__);
  // Return a setup menu in case the plugin supports one.
  return new cSatipPluginSetup();
}

int cPluginSatip::ParseFilters(const char *valueP, int *filtersP)
{
  debug("cPluginSatip::%s(%s)", __FUNCTION__, valueP);
  char buffer[256];
  int n = 0;
  while (valueP && *valueP && (n < SECTION_FILTER_TABLE_SIZE)) {
    strn0cpy(buffer, valueP, sizeof(buffer));
    int i = atoi(buffer);
    //debug("cPluginSatip::%s(): filters[%d]=%d", __FUNCTION__, n, i);
    if (i >= 0)
       filtersP[n++] = i;
    if ((valueP = strchr(valueP, ' ')) != NULL)
       valueP++;
    }
  return n;
}

bool cPluginSatip::SetupParse(const char *nameP, const char *valueP)
{
  debug("cPluginSatip::%s()", __FUNCTION__);
  // Parse your own setup parameters and store their values.
  if (!strcasecmp(nameP, "OperatingMode"))
     SatipConfig.SetOperatingMode(atoi(valueP));
  else if (!strcasecmp(nameP, "EnableEITScan"))
     SatipConfig.SetEITScan(atoi(valueP));
  else if (!strcasecmp(nameP, "DisabledFilters")) {
     int DisabledFilters[SECTION_FILTER_TABLE_SIZE];
     for (unsigned int i = 0; i < ARRAY_SIZE(DisabledFilters); ++i)
         DisabledFilters[i] = -1;
     unsigned int DisabledFiltersCount = ParseFilters(valueP, DisabledFilters);
     for (unsigned int i = 0; i < DisabledFiltersCount; ++i)
         SatipConfig.SetDisabledFilters(i, DisabledFilters[i]);
     }
  else
     return false;
  return true;
}

bool cPluginSatip::Service(const char *idP, void *dataP)
{
  debug("cPluginSatip::%s()", __FUNCTION__);
  return false;
}

const char **cPluginSatip::SVDRPHelpPages(void)
{
  debug("cPluginSatip::%s()", __FUNCTION__);
  static const char *HelpPages[] = {
    "INFO [ <page> ] [ <card index> ]\n"
    "    Prints SAT>IP device information and statistics.\n"
    "    The output can be narrowed using optional \"page\""
    "    option: 1=general 2=pids 3=section filters.\n",
    "MODE\n"
    "    Toggles between bit or byte information mode.\n",
    "LIST\n"
    "    Lists active SAT>IP servers.\n",
    "CONT\n"
    "    Shows SAT>IP device count.\n",
    "OPER\n"
    "    Toggles operating mode of SAT>IP devices.\n",
    NULL
    };
  return HelpPages;
}

cString cPluginSatip::SVDRPCommand(const char *commandP, const char *optionP, int &replyCodeP)
{
  debug("cPluginSatip::%s(%s, %s)", __FUNCTION__, commandP, optionP);
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
        return cString("SAT>IP information not available!");
        }
     }
  else if (strcasecmp(commandP, "MODE") == 0) {
     unsigned int mode = !SatipConfig.GetUseBytes();
     SatipConfig.SetUseBytes(mode);
     return cString::sprintf("SAT>IP information mode: %s\n", mode ? "bytes" : "bits");
     }
  else if (strcasecmp(commandP, "LIST") == 0) {
     cString list = cSatipDiscover::GetInstance()->GetServerList();
     if (!isempty(list)) {
        return list;
        }
     else {
        replyCodeP = 550; // Requested action not taken
        return cString("No SAT>IP devices detected!");
        }
     }
  else if (strcasecmp(commandP, "CONT") == 0) {
     return cString::sprintf("SAT>IP device count: %u", cSatipDevice::Count());
     }
  else if (strcasecmp(commandP, "OPER") == 0) {
     cString mode;
     SatipConfig.ToggleOperatingMode();
     switch (SatipConfig.GetOperatingMode()) {
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
     return cString::sprintf("SAT>IP operating mode: %s\n", *mode);
     }

  return NULL;
}

VDRPLUGINCREATOR(cPluginSatip); // Don't touch this!
