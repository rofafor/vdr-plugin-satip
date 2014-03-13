/*
 * setup.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/status.h>
#include <vdr/menu.h>

#include "common.h"
#include "config.h"
#include "device.h"
#include "discover.h"
#include "setup.h"

// --- cSatipServerInfo -------------------------------------------------------

class cSatipServerInfo : public cOsdMenu
{
private:
  cString addressM;
  cString modelM;
  cString descriptionM;
  uint64_t createdM;
  void Setup(void);

public:
  cSatipServerInfo(cSatipServer *serverP);
  virtual ~cSatipServerInfo();
  virtual eOSState ProcessKey(eKeys keyP);
};

cSatipServerInfo::cSatipServerInfo(cSatipServer *serverP)
: cOsdMenu(tr("SAT>IP Device"), 20),
  addressM(serverP ? serverP->Address() : "---"),
  modelM(serverP ? serverP->Model() : "---"),
  descriptionM(serverP ? serverP->Description() : "---"),
  createdM(serverP ? serverP->Created() : 0)
{
  SetMenuCategory(mcSetupPlugins);
  Setup();
  SetHelp(NULL, NULL, NULL, NULL);
}

cSatipServerInfo::~cSatipServerInfo()
{
}

void cSatipServerInfo::Setup(void)
{
  Add(new cOsdItem(cString::sprintf("%s:\t%s", tr("Address"),       *addressM),              osUnknown, false));
  Add(new cOsdItem(cString::sprintf("%s:\t%s", tr("Model"),         *modelM),                osUnknown, false));
  Add(new cOsdItem(cString::sprintf("%s:\t%s", tr("Description"),   *descriptionM),          osUnknown, false));
  Add(new cOsdItem(cString::sprintf("%s:\t%s", tr("Creation date"), *DayDateTime(createdM)), osUnknown, false));
}

eOSState cSatipServerInfo::ProcessKey(eKeys keyP)
{
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk: state = osBack;     break;
       default:  state = osContinue; break;
       }
     }
  return state;
}

// --- cSatipServerItem -------------------------------------------------------

class cSatipServerItem : public cOsdItem {
private:
  cSatipServer *serverM;

public:
  cSatipServerItem(cSatipServer *serverP);
  cSatipServer *Server(void) { return serverM; }
  virtual void SetMenuItem(cSkinDisplayMenu *displayMenuP, int indexP, bool currentP, bool selectableP);
  };

cSatipServerItem::cSatipServerItem(cSatipServer *serverP)
: serverM(serverP)
{
  SetSelectable(true);
  // Must begin with a '#' character!
  SetText(*cString::sprintf("# %s (%s)\t%s", serverM->Address(), serverM->Model(), serverM->Description()));
}

void cSatipServerItem::SetMenuItem(cSkinDisplayMenu *displayMenuP, int indexP, bool currentP, bool selectableP)
{
  if (displayMenuP)
     displayMenuP->SetItem(Text(), indexP, currentP, selectableP);
}

// --- cSatipMenuInfo ---------------------------------------------------------

class cSatipMenuInfo : public cOsdMenu
{
private:
  enum {
    eInfoTimeoutMs = 2000
  };
  cString textM;
  cTimeMs timeoutM;
  unsigned int pageM;
  void UpdateInfo();

public:
  cSatipMenuInfo();
  virtual ~cSatipMenuInfo();
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys keyP);
};

cSatipMenuInfo::cSatipMenuInfo()
: cOsdMenu(tr("SAT>IP Information")),
  textM(""),
  timeoutM(),
  pageM(SATIP_DEVICE_INFO_GENERAL)
{
  SetMenuCategory(mcText);
  timeoutM.Set(eInfoTimeoutMs);
  UpdateInfo();
  SetHelp(tr("General"), tr("Pids"), tr("Filters"), tr("Bits/bytes"));
}

cSatipMenuInfo::~cSatipMenuInfo()
{
}

void cSatipMenuInfo::UpdateInfo()
{
  cSatipDevice *device = cSatipDevice::GetSatipDevice(cDevice::ActualDevice()->CardIndex());
  if (device)
     textM = device->GetInformation(pageM);
  else
     textM = cString(tr("SAT>IP information not available!"));
  Display();
  timeoutM.Set(eInfoTimeoutMs);
}

void cSatipMenuInfo::Display(void)
{
  cOsdMenu::Display();
  DisplayMenu()->SetText(textM, true);
  if (*textM)
     cStatus::MsgOsdTextItem(textM);
}

eOSState cSatipMenuInfo::ProcessKey(eKeys keyP)
{
  switch (int(keyP)) {
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
                  DisplayMenu()->Scroll(NORMALKEY(keyP) == kUp || NORMALKEY(keyP) == kLeft, NORMALKEY(keyP) == kLeft || NORMALKEY(keyP) == kRight);
                  cStatus::MsgOsdTextItem(NULL, NORMALKEY(keyP) == kUp || NORMALKEY(keyP) == kLeft);
                  return osContinue;
    default: break;
    }

  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk:     return osBack;
       case kRed:    pageM = SATIP_DEVICE_INFO_GENERAL;
                     UpdateInfo();
                     break;
       case kGreen:  pageM = SATIP_DEVICE_INFO_PIDS;
                     UpdateInfo();
                     break;
       case kYellow: pageM = SATIP_DEVICE_INFO_FILTERS;
                     UpdateInfo();
                     break;
       case kBlue:   SatipConfig.SetUseBytes(SatipConfig.GetUseBytes() ? 0 : 1);
                     UpdateInfo();
                     break;
       default:      if (timeoutM.TimedOut())
                        UpdateInfo();
                     state = osContinue;
                     break;
       }
     }
  return state;
}

// --- cSatipPluginSetup ------------------------------------------------------

cSatipPluginSetup::cSatipPluginSetup()
: deviceCountM(0),
  operatingModeM(SatipConfig.GetOperatingMode()),
  eitScanM(SatipConfig.GetEITScan()),
  numDisabledFiltersM(SatipConfig.GetDisabledFiltersCount())
{
  debug("cSatipPluginSetup::%s()", __FUNCTION__);
  operatingModeTextsM[cSatipConfig::eOperatingModeOff]    = tr("off");
  operatingModeTextsM[cSatipConfig::eOperatingModeLow]    = tr("low");
  operatingModeTextsM[cSatipConfig::eOperatingModeNormal] = tr("normal");
  operatingModeTextsM[cSatipConfig::eOperatingModeHigh]   = tr("high");
  if (numDisabledFiltersM > SECTION_FILTER_TABLE_SIZE)
     numDisabledFiltersM = SECTION_FILTER_TABLE_SIZE;
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i) {
      disabledFilterIndexesM[i] = SatipConfig.GetDisabledFilters(i);
      disabledFilterNamesM[i] = tr(section_filter_table[i].description);
      }
  SetMenuCategory(mcSetupPlugins);
  Setup();
  SetHelp(trVDR("Button$Scan"), NULL, NULL, trVDR("Button$Info"));
}

void cSatipPluginSetup::Setup(void)
{
  int current = Current();

  Clear();
  helpM.Clear();

  Add(new cMenuEditStraItem(tr("Operating mode"), &operatingModeM, ELEMENTS(operatingModeTextsM), operatingModeTextsM));
  helpM.Append(tr("Define the used operating mode for all SAT>IP devices:\n\noff - devices are disabled\nlow - devices are working at the lowest priority\nnormal - devices are working within normal parameters\nhigh - devices are working at the highest priority"));

  if (operatingModeM) {
     Add(new cMenuEditBoolItem(tr("Enable EPG scanning"), &eitScanM));
     helpM.Append(tr("Define whether the EPG background scanning shall be used.\n\nThis setting disables the automatic EIT scanning functionality for all SAT>IP devices."));

     Add(new cMenuEditIntItem(tr("Disabled filters"), &numDisabledFiltersM, 0, SECTION_FILTER_TABLE_SIZE, tr("none")));
     helpM.Append(tr("Define number of section filters to be disabled.\n\nCertain section filters might cause some unwanted behaviour to VDR such as time being falsely synchronized. By black-listing the filters here useful section data can be left intact for VDR to process."));

     for (int i = 0; i < numDisabledFiltersM; ++i) {
         Add(new cMenuEditStraItem(*cString::sprintf(" %s %d", tr("Filter"), i + 1), &disabledFilterIndexesM[i], SECTION_FILTER_TABLE_SIZE, disabledFilterNamesM));
         helpM.Append(tr("Define an ill-behaving filter to be blacklisted."));
         }
     }
  Add(new cOsdItem(tr("Active SAT>IP devices:"), osUnknown, false));
  helpM.Append("");

  cSatipServers *servers = cSatipDiscover::GetInstance()->GetServers();
  deviceCountM = servers->Count();
  for (cSatipServer *s = servers->First(); s; s = servers->Next(s)) {
      Add(new cSatipServerItem(s));
      helpM.Append("");
      }

  SetCurrent(Get(current));
  Display();
}

eOSState cSatipPluginSetup::DeviceScan(void)
{
  debug("cSatipPluginSetup::%s()", __FUNCTION__);
  cSatipDiscover::GetInstance()->TriggerScan();

  return osContinue;
}

eOSState cSatipPluginSetup::DeviceInfo(void)
{
  debug("cSatipPluginSetup::%s()", __FUNCTION__);
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cSatipServerItem *item = reinterpret_cast<cSatipServerItem *>(Get(Current()));
  if (item && !!cSatipDiscover::GetInstance()->GetServer(item->Server()))
     return AddSubMenu(new cSatipServerInfo(item->Server()));

  return osContinue;
}

eOSState cSatipPluginSetup::ShowInfo(void)
{
  debug("cSatipPluginSetup::%s()", __FUNCTION__);
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  return AddSubMenu(new cSatipMenuInfo());
}

eOSState cSatipPluginSetup::ProcessKey(eKeys keyP)
{
  bool hadSubMenu = HasSubMenu();
  int oldOperatingMode = operatingModeM;
  int oldNumDisabledFilters = numDisabledFiltersM;
  eOSState state = cMenuSetupPage::ProcessKey(keyP);

  // Ugly hack with hardcoded '#' character :(
  const char *p = Get(Current())->Text();
  if (!hadSubMenu && !HasSubMenu() && (*p == '#') && (keyP == kOk))
     return DeviceInfo();

  if (state == osUnknown) {
     switch (keyP) {
       case kRed:  return DeviceScan();
       case kBlue: return ShowInfo();
       case kInfo: if (Current() < helpM.Size())
                      return AddSubMenu(new cMenuText(cString::sprintf("%s - %s '%s'", tr("Help"), trVDR("Plugin"), PLUGIN_NAME_I18N), helpM[Current()]));
       default:    state = osContinue; break;
       }
     }

  if ((keyP == kNone) && (cSatipDiscover::GetInstance()->GetServers()->Count() != deviceCountM))
     Setup();

  if ((keyP != kNone) && ((numDisabledFiltersM != oldNumDisabledFilters) || (operatingModeM != oldOperatingMode))) {
     while ((numDisabledFiltersM < oldNumDisabledFilters) && (oldNumDisabledFilters > 0))
           disabledFilterIndexesM[--oldNumDisabledFilters] = -1;
     Setup();
     }

  return state;
}

void cSatipPluginSetup::StoreFilters(const char *nameP, int *valuesP)
{
  char buffer[SECTION_FILTER_TABLE_SIZE * 4];
  char *q = buffer;
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i) {
      char s[3];
      if (valuesP[i] < 0)
         break;
      if (q > buffer)
         *q++ = ' ';
      snprintf(s, sizeof(s), "%d", valuesP[i]);
      strncpy(q, s, strlen(s));
      q += strlen(s);
      }
  *q = 0;
  debug("cSatipPluginSetup::%s(%s, %s)", __FUNCTION__, nameP, buffer);
  SetupStore(nameP, buffer);
}

void cSatipPluginSetup::Store(void)
{
  // Store values into setup.conf
  SetupStore("OperatingMode", operatingModeM);
  SetupStore("EnableEITScan", eitScanM);
  StoreFilters("DisabledFilters", disabledFilterIndexesM);
  // Update global config
  SatipConfig.SetOperatingMode(operatingModeM);
  SatipConfig.SetEITScan(eitScanM);
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i)
      SatipConfig.SetDisabledFilters(i, disabledFilterIndexesM[i]);
}
