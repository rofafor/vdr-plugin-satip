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

// --- cSatipMenuScan ---------------------------------------------------------

class cSatipMenuScan : public cOsdMenu
{
private:
  enum {
    INFO_TIMEOUT_MS = 2000
  };
  cString textM;
  cTimeMs timeoutM;

public:
  cSatipMenuScan(cSatipServer *serverP);
  virtual ~cSatipMenuScan();
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys keyP);
};

cSatipMenuScan::cSatipMenuScan(cSatipServer *serverP)
: cOsdMenu(tr("SAT>IP Device")),
  textM("")
{
  SetMenuCategory(mcText);
  if (serverP) {
     if (serverP->Model())
        textM = cString::sprintf("%s\nModel:\t%s", *textM, serverP->Model());
     if (serverP->Address())
        textM = cString::sprintf("%s\nAddress:\t%s", *textM, serverP->Address());
     if (serverP->Description())
        textM = cString::sprintf("%s\nDescription:\t%s", *textM, serverP->Description());
     }
  SetHelp(tr("Button$Scan"), NULL, NULL, NULL);
}

cSatipMenuScan::~cSatipMenuScan()
{
}

void cSatipMenuScan::Display(void)
{
  cOsdMenu::Display();
  DisplayMenu()->SetText(textM, true);
  if (*textM)
     cStatus::MsgOsdTextItem(textM);
}

eOSState cSatipMenuScan::ProcessKey(eKeys keyP)
{
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk:  return osBack;
       case kRed:
       default:   state = osContinue;
                  break;
       }
     }
  return state;
}

// --- cSatipMenuInfo ---------------------------------------------------------

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
    INFO_TIMEOUT_MS = 2000
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
  timeoutM.Set(INFO_TIMEOUT_MS);
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
  timeoutM.Set(INFO_TIMEOUT_MS);
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
  eitScanM(SatipConfig.GetEITScan()),
  numDisabledFiltersM(SatipConfig.GetDisabledFiltersCount())
{
  debug("cSatipPluginSetup::%s()", __FUNCTION__);
  if (numDisabledFiltersM > SECTION_FILTER_TABLE_SIZE)
     numDisabledFiltersM = SECTION_FILTER_TABLE_SIZE;
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i) {
      disabledFilterIndexesM[i] = SatipConfig.GetDisabledFilters(i);
      disabledFilterNamesM[i] = tr(section_filter_table[i].description);
      }
  SetMenuCategory(mcSetupPlugins);
  Setup();
  SetHelp(NULL, NULL, NULL, trVDR("Button$Info"));
}

void cSatipPluginSetup::Setup(void)
{
  int current = Current();

  Clear();
  helpM.Clear();

  Add(new cMenuEditBoolItem(tr("Enable EPG scanning"), &eitScanM));
  helpM.Append(tr("Define whether the EPG background scanning shall be used.\n\nThis setting disables the automatic EIT scanning functionality for all SAT>IP devices."));

  Add(new cMenuEditIntItem(tr("Disabled filters"), &numDisabledFiltersM, 0, SECTION_FILTER_TABLE_SIZE, tr("none")));
  helpM.Append(tr("Define number of section filters to be disabled.\n\nCertain section filters might cause some unwanted behaviour to VDR such as time being falsely synchronized. By black-listing the filters here useful section data can be left intact for VDR to process."));

  for (int i = 0; i < numDisabledFiltersM; ++i) {
      Add(new cMenuEditStraItem(*cString::sprintf(" %s %d", tr("Filter"), i + 1), &disabledFilterIndexesM[i], SECTION_FILTER_TABLE_SIZE, disabledFilterNamesM));
      helpM.Append(tr("Define an ill-behaving filter to be blacklisted."));
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

eOSState cSatipPluginSetup::ChannelScan(void)
{
  debug("cSatipPluginSetup::%s()", __FUNCTION__);
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cSatipServerItem *item = reinterpret_cast<cSatipServerItem *>(Get(Current()));
  if (item && cSatipDiscover::GetInstance()->IsValidServer(item->Server()))
     return AddSubMenu(new cSatipMenuScan(item->Server()));

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
  int oldNumDisabledFilters = numDisabledFiltersM;
  eOSState state = cMenuSetupPage::ProcessKey(keyP);

  // Ugly hack with hardcoded '#' character :(
  const char *p = Get(Current())->Text();
  if (!hadSubMenu && !HasSubMenu() && (*p == '#') && (keyP == kOk))
     return ChannelScan();

  if (state == osUnknown) {
     switch (keyP) {
       case kBlue: return ShowInfo();
       case kInfo: if (Current() < helpM.Size())
                      return AddSubMenu(new cMenuText(cString::sprintf("%s - %s '%s'", tr("Help"), trVDR("Plugin"), PLUGIN_NAME_I18N), helpM[Current()]));
       default:    state = osContinue; break;
       }
     }

  if ((keyP == kNone) && (cSatipDiscover::GetInstance()->GetServers()->Count() != deviceCountM))
     Setup();

  if ((keyP != kNone) && (numDisabledFiltersM != oldNumDisabledFilters)) {
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
  SetupStore("EnableEITScan", eitScanM);
  StoreFilters("DisabledFilters", disabledFilterIndexesM);
  // Update global config
  SatipConfig.SetEITScan(eitScanM);
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i)
      SatipConfig.SetDisabledFilters(i, disabledFilterIndexesM[i]);
}
