/*
 * device.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "config.h"
#include "discover.h"
#include "param.h"
#include "device.h"

#define SATIP_MAX_DEVICES MAXDEVICES

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
  isyslog("creating SAT>IP device %d (CardIndex=%d)", deviceIndexM, CardIndex());
  tsBufferM = new cRingBufferLinear(bufsize + 1, TS_SIZE, false,
                                   *cString::sprintf("SAT>IP TS %d", deviceIndexM));
  if (tsBufferM) {
     tsBufferM->SetTimeouts(100, 100);
     tsBufferM->SetIoThrottle();
     pTunerM = new cSatipTuner(*this, tsBufferM->Free());
     }
  // Start section handler
  pSectionFilterHandlerM = new cSatipSectionFilterHandler(deviceIndexM, bufsize + 1);
  StartSectionHandler();
}

cSatipDevice::~cSatipDevice()
{
  debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  // Stop section handler
  StopSectionHandler();
  DELETE_POINTER(pSectionFilterHandlerM);
  DELETE_POINTER(pTunerM);
  DELETE_POINTER(tsBufferM);
}

bool cSatipDevice::Initialize(unsigned int deviceCountP)
{
  debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceCountP);
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
  debug("cSatipDevice::%s()", __FUNCTION__);
  for (int i = 0; i < SATIP_MAX_DEVICES; ++i) {
      if (SatipDevicesS[i])
         SatipDevicesS[i]->CloseDvr();
      }
}

unsigned int cSatipDevice::Count(void)
{
  unsigned int count = 0;
  debug("cSatipDevice::%s()", __FUNCTION__);
  for (unsigned int i = 0; i < SATIP_MAX_DEVICES; ++i) {
      if (SatipDevicesS[i] != NULL)
         count++;
      }
  return count;
}

cSatipDevice *cSatipDevice::GetSatipDevice(int cardIndexP)
{
  //debug("cSatipDevice::%s(%d)", __FUNCTION__, cardIndexP);
  for (unsigned int i = 0; i < SATIP_MAX_DEVICES; ++i) {
      if (SatipDevicesS[i] && (SatipDevicesS[i]->CardIndex() == cardIndexP)) {
         //debug("cSatipDevice::%s(%d): found!", __FUNCTION__, cardIndexP);
         return SatipDevicesS[i];
         }
      }
  return NULL;
}

cString cSatipDevice::GetGeneralInformation(void)
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
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
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return GetPidStatistic();
}

cString cSatipDevice::GetFiltersInformation(void)
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
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
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return ((cSatipDiscover::GetInstance()->GetServerCount() > 0) || (createdM.Elapsed() > eReadyTimeoutMs));
}

cString cSatipDevice::DeviceType(void) const
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return "SAT>IP";
}

cString cSatipDevice::DeviceName(void) const
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return deviceNameM;
}

bool cSatipDevice::AvoidRecording(void) const
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return SatipConfig.IsOperatingModeLow();
}

int cSatipDevice::SignalStrength(void) const
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return (pTunerM ? pTunerM->SignalStrength() : -1);
}

int cSatipDevice::SignalQuality(void) const
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return (pTunerM ? pTunerM->SignalQuality() : -1);
}

bool cSatipDevice::ProvidesSource(int sourceP) const
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
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
  debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return (ProvidesSource(channelP->Source()));
}

bool cSatipDevice::ProvidesChannel(const cChannel *channelP, int priorityP, bool *needsDetachReceiversP) const
{
  bool result = false;
  bool hasPriority = (priorityP == IDLEPRIORITY) || (priorityP > this->Priority());
  bool needsDetachReceivers = false;

  debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);

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
  if (channelP) {
     cDvbTransponderParameters dtp(channelP->Parameters());
     cString params = GetTransponderUrlParameters(channelP);
     if (isempty(params)) {
        error("Unrecognized SAT>IP channel parameters: %s", channelP->Parameters());
        return false;
        }
     cString address;
     cSatipServer *server = cSatipDiscover::GetInstance()->GetServer(channelP->Source(), channelP->Transponder(), dtp.System());
     if (!server) {
        debug("cSatipDevice::%s(%u): no suitable server found", __FUNCTION__, deviceIndexM);
        return false;
        }
     cSatipDiscover::GetInstance()->SetTransponder(server, channelP->Transponder());
     if (pTunerM && pTunerM->SetSource(server, *params, deviceIndexM)) {
        deviceNameM = cString::sprintf("%s %d %s", *DeviceType(), deviceIndexM, *cSatipDiscover::GetInstance()->GetServerString(server));
        channelM = *channelP;
        return true;
        }
     }
  return false;
}

bool cSatipDevice::SetPid(cPidHandle *handleP, int typeP, bool onP)
{
  //debug("cSatipDevice::%s(%u): pid=%d type=%d on=%d", __FUNCTION__, deviceIndexM, handleP->pid, typeP, onP);
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
  //debug("cSatipDevice::%s(%u): pid=%d tid=%d mask=%d", __FUNCTION__, deviceIndexM, pidP, tidP, maskP);
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
  //debug("cSatipDevice::%s(%u): handle=%d", __FUNCTION__, deviceIndexM, handleP);
  if (pSectionFilterHandlerM) {
     if (pTunerM)
        pTunerM->SetPid(pSectionFilterHandlerM->GetPid(handleP), ptOther, false);
     pSectionFilterHandlerM->Close(handleP);
     }
}

bool cSatipDevice::OpenDvr(void)
{
  debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  isPacketDeliveredM = false;
  tsBufferM->Clear();
  if (pTunerM)
     pTunerM->Open();
  isOpenDvrM = true;
  return true;
}

void cSatipDevice::CloseDvr(void)
{
  debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  if (pTunerM)
     pTunerM->Close();
  isOpenDvrM = false;
}

bool cSatipDevice::HasLock(int timeoutMsP) const
{
  //debug("cSatipDevice::%s(%u): timeoutMs=%d", __FUNCTION__, deviceIndexM, timeoutMsP);
  return (pTunerM && pTunerM->HasLock());
}

bool cSatipDevice::HasInternalCam(void)
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return false;
}

void cSatipDevice::WriteData(uchar *bufferP, int lengthP)
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  // Fill up TS buffer
  if (tsBufferM) {
     int len = tsBufferM->Put(bufferP, lengthP);
     if (len != lengthP)
        tsBufferM->ReportOverflow(lengthP - len);
     }
  // Filter the sections
  if (pSectionFilterHandlerM)
     pSectionFilterHandlerM->Write(bufferP, lengthP);
}

unsigned int cSatipDevice::CheckData(void)
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  if (tsBufferM)
     return (unsigned int)tsBufferM->Free();
  return 0;
}

int cSatipDevice::GetId(void)
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  return deviceIndexM;
}

uchar *cSatipDevice::GetData(int *availableP)
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
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
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
  tsBufferM->Del(countP);
  isPacketDeliveredM = false;
  // Update buffer statistics
  AddBufferStatistic(countP, tsBufferM->Available());
}

bool cSatipDevice::GetTSPacket(uchar *&dataP)
{
  //debug("cSatipDevice::%s(%u)", __FUNCTION__, deviceIndexM);
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
  // Reduce cpu load by preventing busylooping
  cCondWait::SleepMs(10);
  dataP = NULL;
  return true;
}
