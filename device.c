/*
 * device.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/menu.h> // cRecordControl

#include "config.h"
#include "discover.h"
#include "log.h"
#include "param.h"
#include "device.h"

static cSatipDevice * SatipDevicesS[SATIP_MAX_DEVICES] = { NULL };

cSatipDevice::cSatipDevice(unsigned int indexP)
: deviceIndexM(indexP),
  isPacketDeliveredM(false),
  isOpenDvrM(false),
  deviceNameM(*cString::sprintf("%s %d", *DeviceType(), deviceIndexM)),
  channelM(),
  createdM(0),
  mutexM()
{
  unsigned int bufsize = (unsigned int)SATIP_BUFFER_SIZE;
  bufsize -= (bufsize % TS_SIZE);
  info("Creating device CardIndex=%d DeviceNumber=%d [device %u]", CardIndex(), DeviceNumber(), deviceIndexM);
  tsBufferM = new cRingBufferLinear(bufsize + 1, TS_SIZE, false,
                                   *cString::sprintf("SATIP#%d TS", deviceIndexM));
  if (tsBufferM) {
     tsBufferM->SetTimeouts(10, 10);
     tsBufferM->SetIoThrottle();
     pTunerM = new cSatipTuner(*this, tsBufferM->Free());
     }
  // Start section handler
  pSectionFilterHandlerM = new cSatipSectionFilterHandler(deviceIndexM, bufsize + 1);
  StartSectionHandler();
}

cSatipDevice::~cSatipDevice()
{
  debug1("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  // Stop section handler
  StopSectionHandler();
  DELETE_POINTER(pSectionFilterHandlerM);
  DELETE_POINTER(pTunerM);
  DELETE_POINTER(tsBufferM);
}

bool cSatipDevice::Initialize(unsigned int deviceCountP)
{
  debug1("%s (%u)", __PRETTY_FUNCTION__, deviceCountP);
  if (deviceCountP > SATIP_MAX_DEVICES)
     deviceCountP = SATIP_MAX_DEVICES;
  for (unsigned int i = 0; i < deviceCountP; ++i)
      SatipDevicesS[i] = new cSatipDevice(i);
  for (unsigned int i = deviceCountP; i < SATIP_MAX_DEVICES; ++i)
      SatipDevicesS[i] = NULL;
  return true;
}

void cSatipDevice::Shutdown(void)
{
  debug1("%s", __PRETTY_FUNCTION__);
  for (int i = 0; i < SATIP_MAX_DEVICES; ++i) {
      if (SatipDevicesS[i])
         SatipDevicesS[i]->CloseDvr();
      }
}

unsigned int cSatipDevice::Count(void)
{
  unsigned int count = 0;
  debug1("%s", __PRETTY_FUNCTION__);
  for (unsigned int i = 0; i < SATIP_MAX_DEVICES; ++i) {
      if (SatipDevicesS[i] != NULL)
         count++;
      }
  return count;
}

cSatipDevice *cSatipDevice::GetSatipDevice(int cardIndexP)
{
  debug16("%s (%d)", __PRETTY_FUNCTION__, cardIndexP);
  for (unsigned int i = 0; i < SATIP_MAX_DEVICES; ++i) {
      if (SatipDevicesS[i] && (SatipDevicesS[i]->CardIndex() == cardIndexP)) {
         debug16("%s (%d): Found!", __PRETTY_FUNCTION__, cardIndexP);
         return SatipDevicesS[i];
         }
      }
  return NULL;
}

cString cSatipDevice::GetSatipStatus(void)
{
  cString info = "";
  for (int i = 0; i < cDevice::NumDevices(); i++) {
      const cDevice *device = cDevice::GetDevice(i);
      if (device && strstr(device->DeviceType(), "SAT>IP")) {
         int timers = 0;
         bool live = (device == cDevice::ActualDevice());
         bool lock = device->HasLock();
         const cChannel *channel = device->GetCurrentlyTunedTransponder();
         for (cTimer *timer = Timers.First(); timer; timer = Timers.Next(timer)) {
             if (timer->Recording()) {
                cRecordControl *control = cRecordControls::GetRecordControl(timer);
                if (control && control->Device() == device)
                   timers++;
                }
            }
         info = cString::sprintf("%sDevice: %s\n", *info, *device->DeviceName());
         if (lock)
            info = cString::sprintf("%sCardIndex: %d  HasLock: yes  Strength: %d  Quality: %d%s\n", *info, device->CardIndex(), device->SignalStrength(), device->SignalQuality(), live ? "  Live: yes" : "");
         else
            info = cString::sprintf("%sCardIndex: %d  HasLock: no\n", *info, device->CardIndex());
         if (channel && channel->Number() > 0)
            info = cString::sprintf("%sTransponder: %d  Channel: %s\n", *info, (channel && channel->Number() > 0) ? channel->Transponder() : 0, (channel && channel->Number() > 0) ? channel->Name() : "---");
         if (timers)
            info = cString::sprintf("%sRecording: %d timer%s\n", *info, timers, (timers > 1) ? "s" : "");
         info = cString::sprintf("%s\n", *info);
         }
      }
  return isempty(*info) ? cString(tr("SAT>IP information not available!")) : info;
}

cString cSatipDevice::GetGeneralInformation(void)
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return cString::sprintf("SAT>IP device: %d\nCardIndex: %d\nStream: %s\nSignal: %s\nStream bitrate: %s\n%sChannel: %s",
                          deviceIndexM, CardIndex(),
                          pTunerM ? *pTunerM->GetInformation() : "",
                          pTunerM ? *pTunerM->GetSignalStatus() : "",
                          pTunerM ? *pTunerM->GetTunerStatistic() : "",
                          *GetBufferStatistic(),
                          *Channels.GetByNumber(cDevice::CurrentChannel())->ToText());
}

cString cSatipDevice::GetPidsInformation(void)
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return GetPidStatistic();
}

cString cSatipDevice::GetFiltersInformation(void)
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return cString::sprintf("Active section filters:\n%s", pSectionFilterHandlerM ? *pSectionFilterHandlerM->GetInformation() : "");
}

cString cSatipDevice::GetInformation(unsigned int pageP)
{
  // generate information string
  cString s;
  switch (pageP) {
    case SATIP_DEVICE_INFO_GENERAL:
         s = GetGeneralInformation();
         break;
    case SATIP_DEVICE_INFO_PIDS:
         s = GetPidsInformation();
         break;
    case SATIP_DEVICE_INFO_FILTERS:
         s = GetFiltersInformation();
         break;
    case SATIP_DEVICE_INFO_PROTOCOL:
         s = pTunerM ? *pTunerM->GetInformation() : "";
         break;
    case SATIP_DEVICE_INFO_BITRATE:
         s = pTunerM ? *pTunerM->GetTunerStatistic() : "";
         break;
    default:
         s = cString::sprintf("%s%s%s",
                              *GetGeneralInformation(),
                              *GetPidsInformation(),
                              *GetFiltersInformation());
         break;
    }
  return s;
}

bool cSatipDevice::Ready(void)
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return ((cSatipDiscover::GetInstance()->GetServerCount() > 0) || (createdM.Elapsed() > eReadyTimeoutMs));
}

cString cSatipDevice::DeviceType(void) const
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return "SAT>IP";
}

cString cSatipDevice::DeviceName(void) const
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return deviceNameM;
}

bool cSatipDevice::AvoidRecording(void) const
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return SatipConfig.IsOperatingModeLow();
}

int cSatipDevice::SignalStrength(void) const
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return (pTunerM ? pTunerM->SignalStrength() : -1);
}

int cSatipDevice::SignalQuality(void) const
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return (pTunerM ? pTunerM->SignalQuality() : -1);
}

bool cSatipDevice::ProvidesSource(int sourceP) const
{
  cSource *s = Sources.Get(sourceP);
  debug9("%s (%c) desc='%s' [device %u]", __PRETTY_FUNCTION__, cSource::ToChar(sourceP), s ? s->Description() : "", deviceIndexM);
  // source descriptions starting with '0' are disabled
  if (s && s->Description() && (*(s->Description()) == '0'))
     return false;
  if (!SatipConfig.IsOperatingModeOff() && !!cSatipDiscover::GetInstance()->GetServer(sourceP)) {
     int numDisabledSourcesM = SatipConfig.GetDisabledSourcesCount();
     for (int i = 0; i < numDisabledSourcesM; ++i) {
         if (sourceP == SatipConfig.GetDisabledSources(i))
            return false;
         }
     return true;
     }
  return false;
}

bool cSatipDevice::ProvidesTransponder(const cChannel *channelP) const
{
  debug9("%s (%d) transponder=%d source=%c [device %u]", __PRETTY_FUNCTION__, channelP ? channelP->Number() : -1, channelP ? channelP->Transponder() : -1, channelP ? cSource::ToChar(channelP->Source()) : '?', deviceIndexM);
  if (!ProvidesSource(channelP->Source()))
     return false;
  return DeviceHooksProvidesTransponder(channelP);
}

bool cSatipDevice::ProvidesChannel(const cChannel *channelP, int priorityP, bool *needsDetachReceiversP) const
{
  bool result = false;
  bool hasPriority = (priorityP == IDLEPRIORITY) || (priorityP > this->Priority());
  bool needsDetachReceivers = false;

  debug9("%s (%d, %d, %d) [device %u]", __PRETTY_FUNCTION__, channelP ? channelP->Number() : -1, priorityP, !!needsDetachReceiversP, deviceIndexM);

  if (channelP && ProvidesTransponder(channelP)) {
     result = hasPriority;
     if (priorityP > IDLEPRIORITY) {
        if (Receiving()) {
           if (IsTunedToTransponder(channelP)) {
              if (channelP->Vpid() && !HasPid(channelP->Vpid()) || channelP->Apid(0) && !HasPid(channelP->Apid(0)) || channelP->Dpid(0) && !HasPid(channelP->Dpid(0))) {
                 if (CamSlot() && channelP->Ca() >= CA_ENCRYPTED_MIN) {
                    if (CamSlot()->CanDecrypt(channelP))
                       result = true;
                    else
                       needsDetachReceivers = true;
                    }
                 else
                    result = true;
                 }
              else
                 result = true;
              }
           else
              needsDetachReceivers = Receiving();
           }
        }
     }
  if (needsDetachReceiversP)
     *needsDetachReceiversP = needsDetachReceivers;
  return result;
}

bool cSatipDevice::ProvidesEIT(void) const
{
  return (SatipConfig.GetEITScan());
}

int cSatipDevice::NumProvidedSystems(void) const
{
  int count = cSatipDiscover::GetInstance()->NumProvidedSystems();
  // Tweak the count according to operation mode
  if (SatipConfig.IsOperatingModeLow())
     count = 15;
  else if (SatipConfig.IsOperatingModeHigh())
     count = 1;
  // Clamp the count between 1 and 15
  if (count > 15)
     count = 15;
  else if (count < 1)
     count = 1;
  return count;
}

const cChannel *cSatipDevice::GetCurrentlyTunedTransponder(void) const
{
  return &channelM;
}

bool cSatipDevice::IsTunedToTransponder(const cChannel *channelP) const
{
  if (pTunerM && !pTunerM->IsTuned())
     return false;
  if ((channelM.Source() != channelP->Source()) || (channelM.Transponder() != channelP->Transponder()))
     return false;
  return (strcmp(channelM.Parameters(), channelP->Parameters()) == 0);
}

bool cSatipDevice::MaySwitchTransponder(const cChannel *channelP) const
{
  return cDevice::MaySwitchTransponder(channelP);
}

bool cSatipDevice::SetChannelDevice(const cChannel *channelP, bool liveViewP)
{
  debug9("%s (%d, %d) [device %u]", __PRETTY_FUNCTION__, channelP ? channelP->Number() : -1, liveViewP, deviceIndexM);
  if (channelP) {
     cDvbTransponderParameters dtp(channelP->Parameters());
     cString params = GetTransponderUrlParameters(channelP);
     if (isempty(params)) {
        error("Unrecognized channel parameters: %s [device %u]", channelP->Parameters(), deviceIndexM);
        return false;
        }
     cString address;
     cSatipServer *server = cSatipDiscover::GetInstance()->GetServer(channelP->Source(), channelP->Transponder(), dtp.System());
     if (!server) {
        debug9("%s No suitable server found [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
        return false;
        }
     cSatipDiscover::GetInstance()->SetTransponder(server, channelP->Transponder());
     if (pTunerM && pTunerM->SetSource(server, *params, deviceIndexM)) {
        channelM = *channelP;
        deviceNameM = cString::sprintf("%s %d %s", *DeviceType(), deviceIndexM, *cSatipDiscover::GetInstance()->GetServerString(server));
        return true;
        }
     }
  else if (pTunerM) {
     pTunerM->SetSource(NULL, NULL, deviceIndexM);
     return true;
     }
  return false;
}

bool cSatipDevice::SetPid(cPidHandle *handleP, int typeP, bool onP)
{
  debug12("%s (%d, %d, %d) [device %u]", __PRETTY_FUNCTION__, handleP->pid, typeP, onP, deviceIndexM);
  if (pTunerM && handleP && handleP->pid >= 0) {
     if (onP)
        return pTunerM->SetPid(handleP->pid, typeP, true);
     else if (!handleP->used)
        return pTunerM->SetPid(handleP->pid, typeP, false);
     }
  return true;
}

int cSatipDevice::OpenFilter(u_short pidP, u_char tidP, u_char maskP)
{
  debug12("%s (%d, %02X, %02X) [device %d]", __PRETTY_FUNCTION__, pidP, tidP, maskP, deviceIndexM);
  if (pSectionFilterHandlerM) {
     int handle = pSectionFilterHandlerM->Open(pidP, tidP, maskP);
     if (pTunerM && (handle >= 0))
        pTunerM->SetPid(pidP, ptOther, true);
     return handle;
     }
  return -1;
}

void cSatipDevice::CloseFilter(int handleP)
{
  if (pSectionFilterHandlerM) {
     int pid = pSectionFilterHandlerM->GetPid(handleP);
     debug12("%s (%d) [device %u]", __PRETTY_FUNCTION__, pid, deviceIndexM);
     if (pTunerM)
        pTunerM->SetPid(pid, ptOther, false);
     pSectionFilterHandlerM->Close(handleP);
     }
}

bool cSatipDevice::OpenDvr(void)
{
  debug9("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  isPacketDeliveredM = false;
  tsBufferM->Clear();
  if (pTunerM)
     pTunerM->Open();
  isOpenDvrM = true;
  return true;
}

void cSatipDevice::CloseDvr(void)
{
  debug9("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  if (pTunerM)
     pTunerM->Close();
  isOpenDvrM = false;
}

bool cSatipDevice::HasLock(int timeoutMsP) const
{
  debug16("%s (%d) [device %d]", __PRETTY_FUNCTION__, timeoutMsP, deviceIndexM);
  return (pTunerM && pTunerM->HasLock());
}

bool cSatipDevice::HasInternalCam(void)
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  return SatipConfig.GetCIExtension();
}

void cSatipDevice::WriteData(uchar *bufferP, int lengthP)
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  // Fill up TS buffer
  if (isOpenDvrM && tsBufferM) {
     int len = tsBufferM->Put(bufferP, lengthP);
     if (len != lengthP)
        tsBufferM->ReportOverflow(lengthP - len);
     }
  // Filter the sections
  if (pSectionFilterHandlerM)
     pSectionFilterHandlerM->Write(bufferP, lengthP);
}

int cSatipDevice::GetId(void)
{
  return deviceIndexM;
}

int cSatipDevice::GetPmtPid(void)
{
  int pid = 0;
#if defined(APIVERSNUM) && APIVERSNUM >= 20107
  pid = channelM.Ca() ? ::GetPmtPid(channelM.Source(), channelM.Transponder(), channelM.Sid()) : 0;
#endif
  debug11("%s pmtpid=%d source=%c transponder=%d sid=%d name=%s [device %u]", __PRETTY_FUNCTION__, pid, cSource::ToChar(channelM.Source()), channelM.Transponder(), channelM.Sid(), channelM.Name(), deviceIndexM);
  return pid;
}

int cSatipDevice::GetCISlot(void)
{
  int slot = 0;
  int ca = 0;
  for (const int *id = channelM.Caids(); *id; ++id) {
      if (checkCASystem(SatipConfig.GetCICAM(0), *id)) {
         ca = *id;
         slot = 1;
         break;
         }
      else if (checkCASystem(SatipConfig.GetCICAM(1), *id)) {
         ca = *id;
         slot = 2;
         break;
         }
      }
  debug11("%s slot=%d ca=%X name=%s [device %u]", __PRETTY_FUNCTION__, slot, ca, channelM.Name(), deviceIndexM);
  return slot;
}

uchar *cSatipDevice::GetData(int *availableP)
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  if (isOpenDvrM && tsBufferM) {
     int count = 0;
     if (isPacketDeliveredM)
        SkipData(TS_SIZE);
     uchar *p = tsBufferM->Get(count);
     if (p && count >= TS_SIZE) {
        if (*p != TS_SYNC_BYTE) {
           for (int i = 1; i < count; i++) {
               if (p[i] == TS_SYNC_BYTE) {
                  count = i;
                  break;
                  }
               }
           tsBufferM->Del(count);
           info("Skipped %d bytes to sync on TS packet", count);
           return NULL;
           }
        isPacketDeliveredM = true;
        if (availableP)
           *availableP = count;
        // Update pid statistics
        AddPidStatistic(ts_pid(p), payload(p));
        return p;
        }
     }
  return NULL;
}

void cSatipDevice::SkipData(int countP)
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  tsBufferM->Del(countP);
  isPacketDeliveredM = false;
  // Update buffer statistics
  AddBufferStatistic(countP, tsBufferM->Available());
}

bool cSatipDevice::GetTSPacket(uchar *&dataP)
{
  debug16("%s [device %u]", __PRETTY_FUNCTION__, deviceIndexM);
  if (tsBufferM) {
#if defined(APIVERSNUM) && APIVERSNUM >= 20104
     if (cCamSlot *cs = CamSlot()) {
        if (cs->WantsTsData()) {
           int available;
           dataP = GetData(&available);
           if (dataP) {
              dataP = cs->Decrypt(dataP, available);
              SkipData(available);
              }
           return true;
           }
        }
#endif
     dataP = GetData();
     return true;
     }
  dataP = NULL;
  return true;
}
