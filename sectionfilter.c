/*
 * sectionfilter.c: SAT>IP plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "config.h"
#include "log.h"
#include "sectionfilter.h"

cSatipSectionFilter::cSatipSectionFilter(int deviceIndexP, uint16_t pidP, uint8_t tidP, uint8_t maskP)
: pusiSeenM(0),
  feedCcM(0),
  doneqM(0),
  secBufM(NULL),
  secBufpM(0),
  secLenM(0),
  tsFeedpM(0),
  pidM(pidP),
  ringBufferM(new cRingBufferFrame(eDmxMaxSectionCount * eDmxMaxSectionSize)),
  deviceIndexM(deviceIndexP)
{
  debug16("%s (%d, %d, %d, %d) [device %d]", __PRETTY_FUNCTION__, deviceIndexM, pidM, tidP, maskP, deviceIndexM);
  int i;

  memset(secBufBaseM,     0, sizeof(secBufBaseM));
  memset(filterValueM,    0, sizeof(filterValueM));
  memset(filterMaskM,     0, sizeof(filterMaskM));
  memset(filterModeM,     0, sizeof(filterModeM));
  memset(maskAndModeM,    0, sizeof(maskAndModeM));
  memset(maskAndNotModeM, 0, sizeof(maskAndNotModeM));

  filterValueM[0] = tidP;
  filterMaskM[0] = maskP;

  // Invert the filter
  for (i = 0; i < eDmxMaxFilterSize; ++i)
      filterValueM[i] ^= 0xFF;

  uint8_t doneq = 0;
  for (i = 0; i < eDmxMaxFilterSize; ++i) {
      uint8_t mode = filterModeM[i];
      uint8_t mask = filterMaskM[i];
      maskAndModeM[i] = (uint8_t)(mask & mode);
      maskAndNotModeM[i] = (uint8_t)(mask & ~mode);
      doneq |= maskAndNotModeM[i];
      }
  doneqM = doneq ? 1 : 0;

  // Create sockets
  socketM[0] = socketM[1] = -1;
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socketM) != 0) {
     char tmp[64];
     error("Opening section filter sockets failed (device=%d pid=%d): %s", deviceIndexM, pidM, strerror_r(errno, tmp, sizeof(tmp)));
     }
  else if ((fcntl(socketM[0], F_SETFL, O_NONBLOCK) != 0) || (fcntl(socketM[1], F_SETFL, O_NONBLOCK) != 0)) {
     char tmp[64];
     error("Setting section filter socket to non-blocking mode failed (device=%d pid=%d): %s", deviceIndexM, pidM, strerror_r(errno, tmp, sizeof(tmp)));
     }
}

cSatipSectionFilter::~cSatipSectionFilter()
{
  debug16("%s pid=%d [device %d]", __PRETTY_FUNCTION__, pidM, deviceIndexM);
  int tmp = socketM[1];
  socketM[1] = -1;
  if (tmp >= 0)
     close(tmp);
  tmp = socketM[0];
  socketM[0] = -1;
  if (tmp >= 0)
     close(tmp);
  secBufM = NULL;
  DELETENULL(ringBufferM);
}

inline uint16_t cSatipSectionFilter::GetLength(const uint8_t *dataP)
{
  return (uint16_t)(3 + ((dataP[1] & 0x0f) << 8) + dataP[2]);
}

void cSatipSectionFilter::New(void)
{
  tsFeedpM = secBufpM = secLenM = 0;
  secBufM = secBufBaseM;
}

int cSatipSectionFilter::Filter(void)
{
  if (secBufM) {
     int i;
     uint8_t neq = 0;

     for (i = 0; i < eDmxMaxFilterSize; ++i) {
         uint8_t calcxor = (uint8_t)(filterValueM[i] ^ secBufM[i]);
         if (maskAndModeM[i] & calcxor)
            return 0;
         neq |= (maskAndNotModeM[i] & calcxor);
         }

     if (doneqM && !neq)
        return 0;

     if (ringBufferM && (secLenM > 0)) {
        cFrame* section = new cFrame(secBufM, secLenM);
        if (!ringBufferM->Put(section))
           DELETE_POINTER(section);
        }
     }
  return 0;
}

inline int cSatipSectionFilter::Feed(void)
{
  if (Filter() < 0)
     return -1;
  secLenM = 0;
  return 0;
}

int cSatipSectionFilter::CopyDump(const uint8_t *bufP, uint8_t lenP)
{
  uint16_t limit, n;

  if (tsFeedpM >= eDmxMaxSectionFeedSize)
     return 0;

  if (tsFeedpM + lenP > eDmxMaxSectionFeedSize)
     lenP = (uint8_t)(eDmxMaxSectionFeedSize - tsFeedpM);

  if (lenP == 0)
     return 0;

  memcpy(secBufBaseM + tsFeedpM, bufP, lenP);
  tsFeedpM = uint16_t(tsFeedpM + lenP);

  limit = tsFeedpM;
  if (limit > eDmxMaxSectionFeedSize)
     return -1; // internal error should never happen

  // Always set secbuf
  secBufM = secBufBaseM + secBufpM;

  for (n = 0; secBufpM + 2 < limit; ++n) {
      uint16_t seclen = GetLength(secBufM);
      if ((seclen > eDmxMaxSectionSize) || ((seclen + secBufpM) > limit))
         return 0;
      secLenM = seclen;
      if (pusiSeenM)
         Feed();
      secBufpM = uint16_t(secBufpM + seclen);
      secBufM += seclen;
      }
  return 0;
}

void cSatipSectionFilter::Process(const uint8_t* dataP)
{
  if (dataP[0] != TS_SYNC_BYTE)
     return;

  // Stop if not the PID this filter is looking for
  if (ts_pid(dataP) != pidM)
     return;

  uint8_t count = payload(dataP);

  // Check if no payload or out of range
  if (count == 0)
     return;

  // Payload start
  uint8_t p = (uint8_t)(TS_SIZE - count);

  uint8_t cc = (uint8_t)(dataP[3] & 0x0f);
  int ccok = ((feedCcM + 1) & 0x0f) == cc;
  feedCcM = cc;

  int dc_i = 0;
  if (dataP[3] & 0x20) {
     // Adaption field present, check for discontinuity_indicator
     if ((dataP[4] > 0) && (dataP[5] & 0x80))
        dc_i = 1;
     }

  if (!ccok || dc_i) {
     // Discontinuity detected. Reset pusiSeenM = 0 to
     // stop feeding of suspicious data until next PUSI=1 arrives
     pusiSeenM = 0;
     New();
     }

  if (dataP[1] & 0x40) {
     // PUSI=1 (is set), section boundary is here
     if (count > 1 && dataP[p] < count) {
        const uint8_t *before = &dataP[p + 1];
        uint8_t before_len = dataP[p];
        const uint8_t *after = &before[before_len];
        uint8_t after_len = (uint8_t)(count - 1 - before_len);
        CopyDump(before, before_len);

        // Before start of new section, set pusiSeenM = 1
        pusiSeenM = 1;
        New();
        CopyDump(after, after_len);
        }
     }
  else {
     // PUSI=0 (is not set), no section boundary
     CopyDump(&dataP[p], count);
     }
}

void cSatipSectionFilter::Send(void)
{
  cFrame *section = ringBufferM->Get();
  if (section) {
     uchar *data = section->Data();
     int count = section->Count();
     if (data && (count > 0) && (socketM[1] >= 0) && (socketM[0] >= 0)) {
        if (send(socketM[1], data, count, MSG_EOR) > 0) {
           // Update statistics
           AddSectionStatistic(count, 1);
           }
        else if (errno != EAGAIN)
          error("failed to send section data (%i bytes) [device=%d]", count, deviceIndexM);
        }
     ringBufferM->Drop(section);
     }
}

int cSatipSectionFilter::Available(void) const
{
  return ringBufferM->Available();
}

cSatipSectionFilterHandler::cSatipSectionFilterHandler(int deviceIndexP, unsigned int bufferLenP)
: cThread(cString::sprintf("SATIP#%d section handler", deviceIndexP)),
  ringBufferM(new cRingBufferLinear(bufferLenP, TS_SIZE, false, *cString::sprintf("SATIP %d section handler", deviceIndexP))),
  mutexM(),
  deviceIndexM(deviceIndexP)
{
  debug1("%s (%d, %d) [device %d]", __PRETTY_FUNCTION__, deviceIndexM, bufferLenP, deviceIndexM);

  // Initialize filter pointers
  memset(filtersM, 0, sizeof(filtersM));

  // Create input buffer
  if (ringBufferM) {
     ringBufferM->SetTimeouts(100, 100);
     ringBufferM->SetIoThrottle();
     }
  else
     error("Failed to allocate buffer for section filter handler [device=%d]", deviceIndexM);

  Start();
}

cSatipSectionFilterHandler::~cSatipSectionFilterHandler()
{
  debug1("%s [device %d]", __PRETTY_FUNCTION__, deviceIndexM);
  // Stop thread
  if (Running())
     Cancel(3);
  DELETE_POINTER(ringBufferM);

  // Destroy all filters
  cMutexLock MutexLock(&mutexM);
  for (int i = 0; i < eMaxSecFilterCount; ++i)
      Delete(i);
}

void cSatipSectionFilterHandler::SendAll(void)
{
  cMutexLock MutexLock(&mutexM);
  bool pendingData;
  do {
     pendingData = false;

     // zero polling structures
     memset(pollFdsM, 0, sizeof(pollFdsM));

     // assemble all handlers to poll (use -1 to ignore handlers)
     for (unsigned int i = 0; i < eMaxSecFilterCount; ++i) {
         if (filtersM[i] && filtersM[i]->Available() != 0) {
            pollFdsM[i].fd = filtersM[i]->GetFd();
            pollFdsM[i].events = POLLOUT;
            pendingData = true;
            }
         else
            pollFdsM[i].fd = -1;
         }

     // exit if there isn't any pending data or we time out
     if (!pendingData || poll(pollFdsM, eMaxSecFilterCount, eSecFilterSendTimeoutMs) <= 0)
        return;

     // send data (if available)
     for (unsigned int i = 0; i < eMaxSecFilterCount && pendingData; ++i) {
         if (pollFdsM[i].revents & POLLOUT)
            filtersM[i]->Send();
         }
  } while (pendingData);
}

void cSatipSectionFilterHandler::Action(void)
{
  debug1("%s Entering [device %d]", __PRETTY_FUNCTION__, deviceIndexM);
  // Do the thread loop
  while (Running()) {
        uchar *p = NULL;
        int len = 0;
        // Process all pending TS packets
        while ((p  = ringBufferM->Get(len)) != NULL) {
              if (p && (len >= TS_SIZE)) {
                 if (*p != TS_SYNC_BYTE) {
                    for (int i = 1; i < len; ++i) {
                        if (p[i] == TS_SYNC_BYTE) {
                           len = i;
                           break;
                           }
                        }
                    ringBufferM->Del(len);
                    debug1("%s Skipped %d bytes to sync on TS packet [device %d]", __PRETTY_FUNCTION__, len, deviceIndexM);
                    continue;
                    }
                    // Process TS packet through all filters
                    mutexM.Lock();
                    for (unsigned int i = 0; i < eMaxSecFilterCount; ++i) {
                        if (filtersM[i])
                           filtersM[i]->Process(p);
                        }
                    mutexM.Unlock();
                    ringBufferM->Del(TS_SIZE);
                    }
                }

        // Send demuxed section packets through all filters
        SendAll();
        }
  debug1("%s Exiting [device %d]", __PRETTY_FUNCTION__, deviceIndexM);
}

cString cSatipSectionFilterHandler::GetInformation(void)
{
  debug16("%s [device %d]", __PRETTY_FUNCTION__, deviceIndexM);
  // loop through active section filters
  cMutexLock MutexLock(&mutexM);
  cString s = "";
  unsigned int count = 0;
  for (unsigned int i = 0; i < eMaxSecFilterCount; ++i) {
      if (filtersM[i]) {
         s = cString::sprintf("%sFilter %d: %s Pid=0x%02X (%s)\n", *s, i,
                              *filtersM[i]->GetSectionStatistic(), filtersM[i]->GetPid(),
                              id_pid(filtersM[i]->GetPid()));
         if (++count > SATIP_STATS_ACTIVE_FILTERS_COUNT)
            break;
         }
      }
  return s;
}

bool cSatipSectionFilterHandler::Exists(u_short pidP)
{
  debug16("%s (%d) [device %d]", __PRETTY_FUNCTION__, pidP, deviceIndexM);
  cMutexLock MutexLock(&mutexM);
  for (unsigned int i = 0; i < eMaxSecFilterCount; ++i) {
      if (filtersM[i] && (pidP == filtersM[i]->GetPid())) {
         debug12("%s (%d) Found [device %d]", __PRETTY_FUNCTION__, pidP, deviceIndexM);
         return true;
         }
      }
  return false;
}

bool cSatipSectionFilterHandler::Delete(unsigned int indexP)
{
  debug16("%s (%d) [device %d]", __PRETTY_FUNCTION__, indexP, deviceIndexM);
  if ((indexP < eMaxSecFilterCount) && filtersM[indexP]) {
     debug8("%s (%d) Found [device %d]", __PRETTY_FUNCTION__, indexP, deviceIndexM);
     cSatipSectionFilter *tmp = filtersM[indexP];
     filtersM[indexP] = NULL;
     delete tmp;
     return true;
     }
  return false;
}

bool cSatipSectionFilterHandler::IsBlackListed(u_short pidP, u_char tidP, u_char maskP) const
{
  debug16("%s (%d, %02X, %02X) [device %d]", __PRETTY_FUNCTION__, pidP, tidP, maskP, deviceIndexM);
  // loop through section filter table
  for (int i = 0; i < SECTION_FILTER_TABLE_SIZE; ++i) {
      int index = SatipConfig.GetDisabledFilters(i);
      // Check if matches
      if ((index >= 0) && (index < SECTION_FILTER_TABLE_SIZE) &&
          (section_filter_table[index].pid == pidP) && (section_filter_table[index].tid == tidP) &&
          (section_filter_table[index].mask == maskP)) {
         debug8("%s Found %s [device %d]", __PRETTY_FUNCTION__, section_filter_table[index].description, deviceIndexM);
         return true;
         }
      }
  return false;
}

int cSatipSectionFilterHandler::Open(u_short pidP, u_char tidP, u_char maskP)
{
  cMutexLock MutexLock(&mutexM);
  // Blacklist check, refuse certain filters
  if (IsBlackListed(pidP, tidP, maskP))
     return -1;
  // Search the next free filter slot
  for (unsigned int i = 0; i < eMaxSecFilterCount; ++i) {
      if (!filtersM[i]) {
         filtersM[i] = new cSatipSectionFilter(deviceIndexM, pidP, tidP, maskP);
         debug16("%s (%d, %02X, %02X) handle=%d index=%u [device %d]", __PRETTY_FUNCTION__, pidP, tidP, maskP, filtersM[i]->GetFd(), i, deviceIndexM);
         return filtersM[i]->GetFd();
         }
      }
  // No free filter slot found
  return -1;
}

void cSatipSectionFilterHandler::Close(int handleP)
{
  cMutexLock MutexLock(&mutexM);
  // Search the filter for deletion
  for (unsigned int i = 0; i < eMaxSecFilterCount; ++i) {
      if (filtersM[i] && (handleP == filtersM[i]->GetFd())) {
         debug8("%s (%d) pid=%d index=%u [device %d]", __PRETTY_FUNCTION__, handleP, filtersM[i]->GetPid(), i, deviceIndexM);
         Delete(i);
         break;
         }
      }
}

int cSatipSectionFilterHandler::GetPid(int handleP)
{
  cMutexLock MutexLock(&mutexM);
  // Search the filter for data
  for (unsigned int i = 0; i < eMaxSecFilterCount; ++i) {
      if (filtersM[i] && (handleP == filtersM[i]->GetFd())) {
         debug8("%s (%d) pid=%d index=%u [device %d]", __PRETTY_FUNCTION__, handleP, filtersM[i]->GetPid(), i, deviceIndexM);
         return filtersM[i]->GetPid();
         }
      }
  return -1;
}

void cSatipSectionFilterHandler::Write(uchar *bufferP, int lengthP)
{
  debug16("%s (, %d) [device %d]", __PRETTY_FUNCTION__, lengthP, deviceIndexM);
  // Fill up the buffer
  if (ringBufferM) {
     int len = ringBufferM->Put(bufferP, lengthP);
     if (len != lengthP)
        ringBufferM->ReportOverflow(lengthP - len);
     }
}
