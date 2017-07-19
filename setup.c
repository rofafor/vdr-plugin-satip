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
#include "log.h"
#include "setup.h"

// --- cSatipEditSrcItem ------------------------------------------------------
// This class is a 99% copy of cMenuEditSrcItem() taken from VDR's menu.c

class cSatipEditSrcItem : public cMenuEditIntItem {
private:
  const cSource *source;
protected:
  virtual void Set(void);
public:
  cSatipEditSrcItem(const char *Name, int *Value);
  eOSState ProcessKey(eKeys Key);
  };

cSatipEditSrcItem::cSatipEditSrcItem(const char *Name, int *Value)
:cMenuEditIntItem(Name, Value, 0)
{
  source = Sources.Get(*Value);
  if (!source)
     source = Sources.First();
  Set();
}

void cSatipEditSrcItem::Set(void)
{
  if (source)
     SetValue(cString::sprintf("%s - %s", *cSource::ToString(source->Code()), source->Description()));
  else
     cMenuEditIntItem::Set();
}

eOSState cSatipEditSrcItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     bool IsRepeat = Key & k_Repeat;
     Key = NORMALKEY(Key);
     if (Key == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (source) {
           if (source->Prev())
              source = (cSource *)source->Prev();
           else if (!IsRepeat)
              source = Sources.Last();
           *value = source->Code();
           }
        }
     else if (Key == kRight) {
        if (source) {
           if (source->Next())
              source = (cSource *)source->Next();
           else if (!IsRepeat)
              source = Sources.First();
           }
        else
           source = Sources.First();
        if (source)
           *value = source->Code();
        }
     else
        return state; // we don't call cMenuEditIntItem::ProcessKey(Key) here since we don't accept numerical input
     Set();
     state = osContinue;
     }
  return state;
}

// --- cSatipServerInfo -------------------------------------------------------

class cSatipServerInfo : public cOsdMenu
{
private:
  cSatipServer *serverM;
  int activeM;
  cString addressM;
  cString modelM;
  cString descriptionM;
  cString ciExtensionM;
  uint64_t createdM;
  void Setup(void);

public:
  explicit cSatipServerInfo(cSatipServer *serverP);
  virtual ~cSatipServerInfo();
  virtual eOSState ProcessKey(eKeys keyP);
};

cSatipServerInfo::cSatipServerInfo(cSatipServer *serverP)
: cOsdMenu(tr("SAT>IP Server"), 20),
  serverM(serverP),
  activeM(serverP && serverP->IsActive()),
  addressM(serverP ? (isempty(serverP->SrcAddress()) ? serverP->Address() : *cString::sprintf("%s@%s", serverP->SrcAddress(), serverP->Address())) : "---"),
  modelM(serverP ? serverP->Model() : "---"),
  descriptionM(serverP ? serverP->Description() : "---"),
  ciExtensionM(serverP && serverP->HasCI() ? trVDR("yes") : trVDR("no")),
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
  Add(new cMenuEditBoolItem(trVDR("Active"), &activeM));
  Add(new cOsdItem(cString::sprintf("%s:\t%s", tr("Address"),       *addressM),              osUnknown, false));
  Add(new cOsdItem(cString::sprintf("%s:\t%s", tr("Model"),         *modelM),                osUnknown, false));
  Add(new cOsdItem(cString::sprintf("%s:\t%s", tr("Description"),   *descriptionM),          osUnknown, false));
  Add(new cOsdItem(cString::sprintf("%s:\t%s", tr("CI extension"),  *ciExtensionM),          osUnknown, false));
  Add(new cOsdItem(cString::sprintf("%s:\t%s", tr("Creation date"), *DayDateTime(createdM)), osUnknown, false));
}

eOSState cSatipServerInfo::ProcessKey(eKeys keyP)
{
  int oldActive = activeM;
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk: state = osBack;     break;
       default:  state = osContinue; break;
       }
     }

  if (keyP != kNone && oldActive != activeM) {
     cSatipDiscover::GetInstance()->ActivateServer(serverM, activeM);
     Setup();
     }

  return state;
}

// --- cSatipServerItem -------------------------------------------------------

class cSatipServerItem : public cOsdItem {
private:
  cSatipServer *serverM;

public:
  explicit cSatipServerItem(cSatipServer *serverP);
  cSatipServer *Server(void) { return serverM; }
  virtual void SetMenuItem(cSkinDisplayMenu *displayMenuP, int indexP, bool currentP, bool selectableP);
  };

cSatipServerItem::cSatipServerItem(cSatipServer *serverP)
: serverM(serverP)
{
  SetSelectable(true);
  // Must begin with a '#' character!
  SetText(*cString::sprintf("%s %s (%s)\t%s", serverM->IsActive() ? "+" : "-", isempty(serverM->SrcAddress()) ? serverM->Address() : *cString::sprintf("%s@%s", serverM->SrcAddress(), serverM->Address()), serverM->Model(), serverM->Description()));
}

void cSatipServerItem::SetMenuItem(cSkinDisplayMenu *displayMenuP, int indexP, bool currentP, bool selectableP)
{
  if (displayMenuP)
     displayMenuP->SetItem(Text(), indexP, currentP, selectableP);
}

// --- cSatipMenuDeviceStatus -------------------------------------------------

class cSatipMenuDeviceStatus : public cOsdMenu
{
private:
  enum {
    eInfoTimeoutMs = 15000
  };
  cString textM;
  cTimeMs timeoutM;
  void UpdateInfo();

public:
  cSatipMenuDeviceStatus();
  virtual ~cSatipMenuDeviceStatus();
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys keyP);
};

cSatipMenuDeviceStatus::cSatipMenuDeviceStatus()
: cOsdMenu(tr("SAT>IP Device Status")),
  textM(""),
  timeoutM()
{
  SetMenuCategory(mcText);
  timeoutM.Set(eInfoTimeoutMs);
  UpdateInfo();
  SetHelp(NULL, NULL, NULL, NULL);
}

cSatipMenuDeviceStatus::~cSatipMenuDeviceStatus()
{
}

void cSatipMenuDeviceStatus::UpdateInfo()
{
  textM = cSatipDevice::GetSatipStatus();
  Display();
  timeoutM.Set(eInfoTimeoutMs);
}

void cSatipMenuDeviceStatus::Display(void)
{
  cOsdMenu::Display();
  DisplayMenu()->SetText(textM, true);
  if (*textM)
     cStatus::MsgOsdTextItem(textM);
}

eOSState cSatipMenuDeviceStatus::ProcessKey(eKeys keyP)
{
  eOSState state = cOsdMenu::ProcessKey(keyP);

  if (state == osUnknown) {
     switch (keyP) {
       case kOk: state = osBack;     break;
       default:  if (timeoutM.TimedOut())
                    UpdateInfo();
                 state = osContinue;
                 break;
       }
     }
  return state;
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
: detachedModeM(SatipConfig.GetDetachedMode()),
  deviceCountM(0),
  operatingModeM(SatipConfig.GetOperatingMode()),
  transportModeM(SatipConfig.GetTransportMode()),
  ciExtensionM(SatipConfig.GetCIExtension()),
  eitScanM(SatipConfig.GetEITScan()),
  numDisabledSourcesM(SatipConfig.GetDisabledSourcesCount()),
  numDisabledFiltersM(SatipConfig.GetDisabledFiltersCount())
{
  debug1("%s", __PRETTY_FUNCTION__);
  operatingModeTextsM[cSatipConfig::eOperatingModeOff]    = tr("off");
  operatingModeTextsM[cSatipConfig::eOperatingModeLow]    = tr("low");
  operatingModeTextsM[cSatipConfig::eOperatingModeNormal] = tr("normal");
  operatingModeTextsM[cSatipConfig::eOperatingModeHigh]   = tr("high");
  transportModeTextsM[cSatipConfig::eTransportModeUnicast]    = tr("Unicast");
  transportModeTextsM[cSatipConfig::eTransportModeMulticast]  = tr("Multicast");
  transportModeTextsM[cSatipConfig::eTransportModeRtpOverTcp] = tr("RTP-over-TCP");
  for (unsigned int i = 0; i < ELEMENTS(cicamsM); ++i)
      cicamsM[i] = SatipConfig.GetCICAM(i);
  for (unsigned int i = 0; i < ELEMENTS(ca_systems_table); ++i)
      cicamTextsM[i] = ca_systems_table[i].description;
  if (numDisabledSourcesM > MAX_DISABLED_SOURCES_COUNT)
     numDisabledSourcesM = MAX_DISABLED_SOURCES_COUNT;
  for (int i = 0; i < MAX_DISABLED_SOURCES_COUNT; ++i)
      disabledSourcesM[i] = SatipConfig.GetDisabledSources(i);
  if (numDisabledFiltersM > SECTION_FILTER_TABLE_SIZE)
     numDisabledFiltersM = SECTION_FILTER_TABLE_SIZE;
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i) {
      disabledFilterIndexesM[i] = SatipConfig.GetDisabledFilters(i);
      disabledFilterNamesM[i] = tr(section_filter_table[i].description);
      }
  SetMenuCategory(mcSetupPlugins);
  Setup();
  SetHelp(trVDR("Button$Scan"), NULL, tr("Button$Devices"), trVDR("Button$Info"));
}

void cSatipPluginSetup::Setup(void)
{
  int current = Current();

  Clear();
  helpM.Clear();

  Add(new cMenuEditStraItem(tr("Operating mode"), &operatingModeM, ELEMENTS(operatingModeTextsM), operatingModeTextsM));
  helpM.Append(tr("Define the used operating mode for all SAT>IP devices:\n\noff - devices are disabled\nlow - devices are working at the lowest priority\nnormal - devices are working within normal parameters\nhigh - devices are working at the highest priority"));

  if (operatingModeM) {
     Add(new cMenuEditBoolItem(tr("Enable CI extension"), &ciExtensionM));
     helpM.Append(tr("Define whether a CI extension shall be used.\n\nThis setting enables integrated CI/CAM handling found in some SAT>IP hardware (e.g. Digital Devices OctopusNet)."));

     for (unsigned int i = 0; ciExtensionM && i < ELEMENTS(cicamsM); ++i) {
         Add(new cMenuEditStraItem(*cString::sprintf(" %s #%d", tr("CI/CAM"), i + 1), &cicamsM[i], ELEMENTS(cicamTextsM), cicamTextsM));
         helpM.Append(tr("Define a desired CAM type for the CI slot.\n\nThe '---' option lets SAT>IP hardware do the auto-selection."));
         }

     Add(new cMenuEditBoolItem(tr("Enable EPG scanning"), &eitScanM));
     helpM.Append(tr("Define whether the EPG background scanning shall be used.\n\nThis setting disables the automatic EIT scanning functionality for all SAT>IP devices."));

     Add(new cMenuEditIntItem(tr("Disabled sources"), &numDisabledSourcesM, 0, MAX_DISABLED_SOURCES_COUNT, tr("none")));
     helpM.Append(tr("Define number of sources to be disabled.\n\nSAT>IP servers might not have all satellite positions available and such sources can be blacklisted here."));

     for (int i = 0; i < numDisabledSourcesM; ++i) {
         Add(new cSatipEditSrcItem(*cString::sprintf(" %s %d", trVDR("Source"), i + 1), &disabledSourcesM[i]));
         helpM.Append(tr("Define a source to be blacklisted."));
         }

     Add(new cMenuEditIntItem(tr("Disabled filters"), &numDisabledFiltersM, 0, SECTION_FILTER_TABLE_SIZE, tr("none")));
     helpM.Append(tr("Define number of section filters to be disabled.\n\nCertain section filters might cause some unwanted behaviour to VDR such as time being falsely synchronized. By blacklisting the filters here, useful section data can be left intact for VDR to process."));

     for (int i = 0; i < numDisabledFiltersM; ++i) {
         Add(new cMenuEditStraItem(*cString::sprintf(" %s %d", tr("Filter"), i + 1), &disabledFilterIndexesM[i], SECTION_FILTER_TABLE_SIZE, disabledFilterNamesM));
         helpM.Append(tr("Define an ill-behaving filter to be blacklisted."));
         }
     }
  Add(new cMenuEditStraItem(tr("Transport mode"), &transportModeM, ELEMENTS(transportModeTextsM), transportModeTextsM));
  helpM.Append(tr("Define which transport mode shall be used.\n\nUnicast, Multicast, RTP-over-TCP"));
  Add(new cOsdItem(tr("Active SAT>IP servers:"), osUnknown, false));
  helpM.Append("");

  detachedModeM = SatipConfig.GetDetachedMode();
  if (!detachedModeM) {
     cSatipServers *servers = cSatipDiscover::GetInstance()->GetServers();
     deviceCountM = servers->Count();
     for (cSatipServer *s = servers->First(); s; s = servers->Next(s)) {
         Add(new cSatipServerItem(s));
         helpM.Append("");
         }
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cSatipPluginSetup::DeviceScan(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  cSatipDiscover::GetInstance()->TriggerScan();

  return osContinue;
}

eOSState cSatipPluginSetup::DeviceInfo(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  cSatipServerItem *item = reinterpret_cast<cSatipServerItem *>(Get(Current()));
  if (item && !!cSatipDiscover::GetInstance()->GetServer(item->Server()))
     return AddSubMenu(new cSatipServerInfo(item->Server()));

  return osContinue;
}

eOSState cSatipPluginSetup::ShowDeviceStatus(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  return AddSubMenu(new cSatipMenuDeviceStatus());
}

eOSState cSatipPluginSetup::ShowInfo(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  if (HasSubMenu() || Count() == 0)
     return osContinue;

  return AddSubMenu(new cSatipMenuInfo());
}

eOSState cSatipPluginSetup::ProcessKey(eKeys keyP)
{
  bool hadSubMenu = HasSubMenu();
  int oldOperatingMode = operatingModeM;
  int oldCiExtension = ciExtensionM;
  int oldNumDisabledSources = numDisabledSourcesM;
  int oldNumDisabledFilters = numDisabledFiltersM;
  eOSState state = cMenuSetupPage::ProcessKey(keyP);

  // Ugly hack with hardcoded '+/-' characters :(
  const char *p = Get(Current())->Text();
  if (!hadSubMenu && !HasSubMenu() && p && (*p == '+' || *p == '-') && (keyP == kOk))
     return DeviceInfo();
  if (hadSubMenu && !HasSubMenu())
     Setup();

  if (state == osUnknown) {
     switch (keyP) {
       case kRed:    return DeviceScan();
       case kYellow: return ShowDeviceStatus();
       case kBlue:   return ShowInfo();
       case kInfo:   if (Current() < helpM.Size())
                        return AddSubMenu(new cMenuText(cString::sprintf("%s - %s '%s'", tr("Help"), trVDR("Plugin"), PLUGIN_NAME_I18N), helpM[Current()]));
       default:      state = osContinue; break;
       }
     }

  if ((keyP == kNone) && (cSatipDiscover::GetInstance()->GetServers()->Count() != deviceCountM))
     Setup();

  if ((keyP != kNone) && ((numDisabledSourcesM != oldNumDisabledSources) || (numDisabledFiltersM != oldNumDisabledFilters) || (operatingModeM != oldOperatingMode) || (ciExtensionM != oldCiExtension) || (detachedModeM != SatipConfig.GetDetachedMode()))) {
     while ((numDisabledSourcesM < oldNumDisabledSources) && (oldNumDisabledSources > 0))
           disabledSourcesM[--oldNumDisabledSources] = cSource::stNone;
     while ((numDisabledFiltersM < oldNumDisabledFilters) && (oldNumDisabledFilters > 0))
           disabledFilterIndexesM[--oldNumDisabledFilters] = -1;
     Setup();
     }

  return state;
}

void cSatipPluginSetup::StoreCicams(const char *nameP, int *cicamsP)
{
  cString buffer = "";
  int n = 0;
  for (int i = 0; i < MAX_CICAM_COUNT; ++i) {
      if (cicamsP[i] < 0)
         break;
      if (n++ > 0)
         buffer = cString::sprintf("%s %d", *buffer, cicamsP[i]);
      else
         buffer = cString::sprintf("%d", cicamsP[i]);
      }
  debug3("%s (%s, %s)", __PRETTY_FUNCTION__, nameP, *buffer);
  SetupStore(nameP, *buffer);
}

void cSatipPluginSetup::StoreSources(const char *nameP, int *sourcesP)
{
  cString buffer = "";
  int n = 0;
  for (int i = 0; i < MAX_DISABLED_SOURCES_COUNT; ++i) {
      if (sourcesP[i] == cSource::stNone)
         break;
      if (n++ > 0)
         buffer = cString::sprintf("%s %s", *buffer, *cSource::ToString(sourcesP[i]));
      else
         buffer = cString::sprintf("%s", *cSource::ToString(sourcesP[i]));
      }
  debug3("%s (%s, %s)", __PRETTY_FUNCTION__, nameP, *buffer);
  SetupStore(nameP, *buffer);
}

void cSatipPluginSetup::StoreFilters(const char *nameP, int *valuesP)
{
  cString buffer = "";
  int n = 0;
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i) {
      if (valuesP[i] < 0)
         break;
      if (n++ > 0)
         buffer = cString::sprintf("%s %d", *buffer, valuesP[i]);
      else
         buffer = cString::sprintf("%d", valuesP[i]);
      }
  debug3("%s (%s, %s)", __PRETTY_FUNCTION__, nameP, *buffer);
  SetupStore(nameP, *buffer);
}

void cSatipPluginSetup::Store(void)
{
  // Store values into setup.conf
  SetupStore("OperatingMode", operatingModeM);
  SetupStore("TransportMode", transportModeM);
  SetupStore("EnableCIExtension", ciExtensionM);
  SetupStore("EnableEITScan", eitScanM);
  StoreCicams("CICAM", cicamsM);
  StoreSources("DisabledSources", disabledSourcesM);
  StoreFilters("DisabledFilters", disabledFilterIndexesM);
  // Update global config
  SatipConfig.SetOperatingMode(operatingModeM);
  SatipConfig.SetTransportMode(transportModeM);
  SatipConfig.SetCIExtension(ciExtensionM);
  SatipConfig.SetEITScan(eitScanM);
  for (int i = 0; i < MAX_CICAM_COUNT; ++i)
      SatipConfig.SetCICAM(i, cicamsM[i]);
  for (int i = 0; i < MAX_DISABLED_SOURCES_COUNT; ++i)
      SatipConfig.SetDisabledSources(i, disabledSourcesM[i]);
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i)
      SatipConfig.SetDisabledFilters(i, disabledFilterIndexesM[i]);
}
