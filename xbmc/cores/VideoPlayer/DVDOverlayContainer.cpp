/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DVDOverlayContainer.h"

#include "DVDCodecs/Overlay/DVDOverlay.h"
#include "DVDInputStreams/DVDInputStreamNavigator.h"

#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <ranges>
#include <set>
#include <vector>

CDVDOverlayContainer::~CDVDOverlayContainer()
{
  Clear();
}

void CDVDOverlayContainer::ProcessAndAddOverlayIfValid(
    const std::shared_ptr<CDVDOverlay>& newOverlay)
{
  std::unique_lock lock(*this);

  auto overlays{m_overlays | std::views::filter(
                                 [startTime = newOverlay->iPTSStartTime](
                                     const std::shared_ptr<CDVDOverlay>& overlay)
                                 {
                                   // If existing overlay has a stop time, only consider those that stop after this new overlay starts
                                   // unless marked for replacement
                                   if (overlay->iPTSStopTime > 0.0 &&
                                       (!overlay->replace || overlay->iPTSStopTime <= startTime))
                                     return false;

                                   // Non ending overlays (stop time of 0) and overlays marked for replacement
                                   // get stopped when this new overlay starts
                                   return overlay->iPTSStartTime <= startTime;
                                 })};

  // Update all found overlays with new end time
  std::ranges::for_each(
      overlays, [startTime = newOverlay->iPTSStartTime](const std::shared_ptr<CDVDOverlay>& overlay)
      { overlay->iPTSStopTime = startTime; });

  // Save new overlay
  m_overlays.emplace_back(newOverlay);
}

VecOverlays* CDVDOverlayContainer::GetOverlays()
{
  return &m_overlays;
}

namespace
{
bool IsOverlayFinished(const std::shared_ptr<CDVDOverlay>& overlay,
                       double pts,
                       std::set<double>& forcedStartTimes)
{
  // Remove non-forced overlays that have expired
  if (!overlay->bForced && overlay->iPTSStopTime <= pts && overlay->iPTSStopTime != 0.0)
    return true;

  // Remove forced overlays replaced by newer ones
  if (overlay->bForced)
  {
    // As forcedStartTimes is a std::set, if there is more than one entry then there is a different start time by definition
    if (!forcedStartTimes.empty() &&
        (forcedStartTimes.size() > 1 ||
         std::fabs(*forcedStartTimes.begin() - overlay->iPTSStartTime) >
             std::numeric_limits<double>::epsilon()))
    {
      return true;
    }

    // If overlay is not removed then store it's start time for comparison with older overlays
    if (overlay->iPTSStartTime < pts)
      forcedStartTimes.insert(overlay->iPTSStartTime);
  }
  return false;
}
} // namespace

void CDVDOverlayContainer::CleanUp(double pts)
{
  std::unique_lock lock(*this);

  std::set<double> forcedStartTimes;
  for (auto it = m_overlays.end(); it != m_overlays.begin();)
  {
    // Loop backwards so that only overlays added later are considered as replacements
    --it;
    if (IsOverlayFinished(*it, pts, forcedStartTimes))
      it = m_overlays.erase(it);
  }
}

void CDVDOverlayContainer::Flush()
{
  std::unique_lock lock(*this);

  // Flush only the overlays marked as flushable
  std::erase_if(m_overlays, [](const std::shared_ptr<CDVDOverlay>& ov)
                { return ov->IsOverlayContainerFlushable(); });
}

void CDVDOverlayContainer::Clear()
{
  std::unique_lock lock(*this);
  m_overlays.clear();
}

size_t CDVDOverlayContainer::GetSize() const
{
  return m_overlays.size();
}

bool CDVDOverlayContainer::ContainsOverlayType(DVDOverlayType type)
{
  std::unique_lock lock(*this);

  return std::ranges::any_of(m_overlays,
                             [type](const auto& overlay) { return overlay->IsOverlayType(type); });
}

/*
 * iAction should be LIBDVDNAV_BUTTON_NORMAL or LIBDVDNAV_BUTTON_CLICKED
 */
void CDVDOverlayContainer::UpdateOverlayInfo(
    const std::shared_ptr<CDVDInputStreamNavigator>& pStream, CDVDDemuxSPU* pSpu, int iAction)
{
  std::unique_lock lock(*this);

  pStream->CheckButtons();

  // Update any forced overlays
  for (auto& overlay : m_overlays)
  {
    if (!overlay->IsOverlayType(DVDOVERLAY_TYPE_SPU))
      continue;

    auto pOverlaySpu{std::static_pointer_cast<CDVDOverlaySpu>(overlay)};

    if (!pOverlaySpu->bForced)
      continue;

    if (pOverlaySpu.use_count() > 1)
    {
      pOverlaySpu = std::make_shared<CDVDOverlaySpu>(*pOverlaySpu);
      overlay = pOverlaySpu;
    }

    if (pStream->GetCurrentButtonInfo(*pOverlaySpu, pSpu, iAction))
      pOverlaySpu->m_textureid = 0;
  }
}
