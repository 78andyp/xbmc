/*
 *  Copyright (C) 2005-2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */
#include "DiscDirectoryHelper.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "URL.h"
#include "guilib/LocalizeStrings.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"
#include "video/VideoInfoTag.h"

#include <iomanip>

void CDiscDirectoryHelper::GetEpisodeTitles(CURL url,
                                            const CFileItem& episode,
                                            CFileItemList& items,
                                            std::vector<CVideoInfoTag> episodesOnDisc,
                                            const std::vector<std::vector<unsigned int>>& clips,
                                            const std::vector<std::vector<unsigned int>>& playlists,
                                            std::map<unsigned int, std::string>& playlist_langs)
{
  // Find our episode on disc
  // Need to differentiate between specials and episodes
  std::vector<CVideoInfoTag> specialsOnDisc;
  bool isSpecial{false};
  unsigned int episodeOffset{0};
  const bool allEpisodes{false};

  for (unsigned int i = 0; i < episodesOnDisc.size(); ++i)
  {
    if (episodesOnDisc[i].m_iSeason > 0 &&
        episodesOnDisc[i].m_iSeason == episode.GetVideoInfoTag()->m_iSeason &&
        episodesOnDisc[i].m_iEpisode == episode.GetVideoInfoTag()->m_iEpisode)
    {
      // Episode found
      episodeOffset = i;
    }
    else if (episodesOnDisc[i].m_iSeason == 0)
    {
      // Special
      specialsOnDisc.emplace_back(episodesOnDisc[i]);

      if (episode.GetVideoInfoTag()->m_iSeason == 0 &&
          episodesOnDisc[i].m_iEpisode == episode.GetVideoInfoTag()->m_iEpisode)
      {
        // Special found
        episodeOffset = static_cast<unsigned int>(specialsOnDisc.size() - 1);
        if (!allEpisodes)
          isSpecial = true;
      }

      // Remove from episode list
      episodesOnDisc.erase(episodesOnDisc.begin() + i);
      --i;
    }
  }

  const unsigned int numEpisodes = static_cast<unsigned int>(episodesOnDisc.size());
  const unsigned int episodeDuration = episode.GetVideoInfoTag()->GetDuration();

  CLog::LogF(LOGDEBUG, "*** Episode Search Start ***");

  CLog::LogF(LOGDEBUG, "Looking for season {} episode {} duration {}",
             episode.GetVideoInfoTag()->m_iSeason, episode.GetVideoInfoTag()->m_iEpisode,
             episode.GetVideoInfoTag()->GetDuration());

  // List episodes expected on disc
  for (const auto& e : episodesOnDisc)
  {
    CLog::LogF(LOGDEBUG, "On disc - season {} episode {} duration {}", e.m_iSeason, e.m_iEpisode,
               e.GetDuration());
  }
  for (const auto& e : specialsOnDisc)
  {
    CLog::LogF(LOGDEBUG, "On disc - special - season {} episode {} duration {}", e.m_iSeason,
               e.m_iEpisode, e.GetDuration());
  }

  // Look for a potential play all playlist (can give episode order)
  //
  // Assumptions
  //   1) Playlist clip count = number of episodes on disc (+1)
  //   2) Each clip will be in at least one other playlist
  //   3) Each clip (bar the last) will be at least MIN_EPISODE_LENGTH long
  //   4) Each playlist containing a clip will have at most one other clip after

  std::vector<unsigned int> playAllPlaylists;
  std::vector<std::pair<int, int>> groups;

  // Only look for groups if enough playlists and more than one episode on disc
  if (playlists.size() >= numEpisodes && numEpisodes > 1)
  {
    for (const auto& playlist : playlists)
    {
      // Find playlists that have a clip count = number of episodes on disc (1)
      if (playlist.size() == numEpisodes + PLAYLIST_CLIP_OFFSET ||
          playlist.size() == numEpisodes + PLAYLIST_CLIP_OFFSET + 1)
      {
        bool allClipsQualify = true;
        for (unsigned int i = PLAYLIST_CLIP_OFFSET; i < PLAYLIST_CLIP_OFFSET + numEpisodes; ++i)
        {
          // Get clip information
          const auto& it = std::ranges::find_if(clips, [&](const std::vector<unsigned int>& x)
                                                { return x[0] == playlist[i]; });
          const unsigned int duration = it->at(1);

          // If clip only appears in one other playlist or is too short this is not a Play All playlist (2)(3)
          if (it->size() < (CLIP_PLAYLIST_OFFSET + 2) || duration < MIN_EPISODE_LENGTH)
          {
            allClipsQualify = false;
            break;
          }

          // Now check the playlists to ensure they start with each clip and have at most only one other clip after (4)
          const unsigned int p = (it->at(CLIP_PLAYLIST_OFFSET) == playlist[0])
                                     ? it->at(CLIP_PLAYLIST_OFFSET + 1)
                                     : it->at(CLIP_PLAYLIST_OFFSET);
          const auto& it2 = std::ranges::find_if(playlists, [&p](const std::vector<unsigned int>& x)
                                                 { return x[0] == p; });
          if (it2->size() > (PLAYLIST_CLIP_OFFSET + 2) ||
              it2->at(PLAYLIST_CLIP_OFFSET) != it->at(0))
          {
            allClipsQualify = false;
            break;
          }
        }
        if (allClipsQualify)
        {
          CLog::LogF(LOGDEBUG, "Potential play all playlist {}", playlist[0]);
          playAllPlaylists.emplace_back(playlist[0]);
        }
      }
    }

    // Look for groups of playlists
    unsigned int length = 1;
    for (unsigned int i = 1; i <= playlists.size(); ++i)
    {
      const int curPlaylist =
          (i == playlists.size())
              ? 0
              : static_cast<int>(playlists[i][0]); // Ensure last group is recognised
      const int prevPlaylist = static_cast<int>(playlists[i - 1][0]);
      if (i == playlists.size() || (curPlaylist - prevPlaylist) != 1)
      {
        if (length == numEpisodes)
        {
          // Found a group of consecutive playlists of at least the number of episodes on disc
          const int firstPlaylist = static_cast<int>(playlists[i - length][0]);
          groups.emplace_back(firstPlaylist, prevPlaylist);
          CLog::LogF(LOGDEBUG, "Playlist group found from {} to {}", firstPlaylist, prevPlaylist);
        }
        length = 1;
      }
      else
      {
        length++;
      }
    }
  }

  // At this stage we have a number of ways of trying to determine the correct playlist for an episode
  //
  // 1) Using a 'Play All' playlist
  //    - Caveat: there may not be one
  //
  // 2) Using the longest (non-'Play All') playlists that are consecutive
  //    - Caveat: assumes play-all detection has worked and playlists are consecutive (hopefully reasonable assumptions)
  //
  // 3) Playlists that are +/- 30sec of the episode length that is passed to us
  //    - Caveat: the episode length may be wrong - either from scraper/NFO or from bug in Kodi that overwrites episode streamdetails on same disc
  //
  // 4) Using position in a group of adjacent playlists (see below)
  //    - Cavet: won't work for single episode discs
  //
  // 5) - For single episode discs - this is hard. There are some discs where there are extras that are longer than the episode itself
  //      The only obvious difference sometimes seems to be number of languages
  //
  // Order of preference:
  //
  // 1) Use 'Play All' playlist - take the nth clip and find a playlist that plays that clip
  //
  // 2) Take the nth playlist of a consecutive run of longest playlists
  //
  // 3) Refine a group using relative position of playlist found on basis of length
  //    - this tries to account for episodes that have the wrong duration passed
  //    eg. if we have a disc with episodes 4,5,6 we would expect episode 5 to be the middle playlist of a group of 3 consecutive playlists (eg. 801, 802, 803)
  //
  // 4) Playlist based on episode length alone (assumes no groups found or 2) and 3) failed)
  //
  // 5) Look at groups found (ignoring length) and pick the relevant playlist - eg. the middle one for episode 5 using the example above
  //    - Only looks at playlists > MIN_EPISODE_LENGTH
  //
  // 6) For single episode discs only, look at the longest playlist with multiple languages
  //
  // SPECIALS
  //
  // These are more difficult - as there may only be one per disc and we can't make assumptions about playlists.
  // So have to look on basis of duration alone.
  //

  std::vector<std::pair<unsigned int, unsigned int>> candidatePlaylists;
  bool foundEpisode{false};
  bool findIdenticalEpisodes{false};
  bool useCommonPlaylist{false};
  std::vector<unsigned int> commonStartingPlaylists = {801, 800, 1};

  if (allEpisodes)
    CLog::LogF(LOGDEBUG, "Looking for all episodes on disc");

  if (playAllPlaylists.size() == 1 && !isSpecial)
  {
    CLog::LogF(LOGDEBUG, "Using Play All playlist method");

    // Get the relevant clip
    const auto& it = std::ranges::find_if(playlists, [&](const std::vector<unsigned int>& x)
                                          { return x[0] == playAllPlaylists[0]; });

    for (unsigned int i = PLAYLIST_CLIP_OFFSET; i < it->size(); ++i)
    {
      if (allEpisodes || i == PLAYLIST_CLIP_OFFSET + episodeOffset)
      {
        const unsigned int clip = it->at(i);
        CLog::LogF(LOGDEBUG, "Clip is {}", clip);

        // Find playlist with starting with that clip (that isn't the play all playlist)
        const auto& it2 = std::ranges::find_if(
            playlists, [&](const std::vector<unsigned int>& x)
            { return (x[PLAYLIST_CLIP_OFFSET] == clip && x[0] != playAllPlaylists[0]); });

        // Ensure playlist starting with clip found (occasionally the play all playlist will have a clip at the end
        // that isn't in other playlists eg. dedicated end credits
        if (it2 != playlists.end())
        {
          const unsigned int playlist = it2->at(0);
          const unsigned int duration = it2->at(1);

          CLog::LogF(LOGDEBUG, "Candidate playlist {} duration {}", playlist, duration);

          candidatePlaylists.emplace_back(playlist, i - PLAYLIST_CLIP_OFFSET);
        }
      }
    }
    foundEpisode = true;
    findIdenticalEpisodes = true;
  }

  // Look for the longest (non-Play All) playlists

  if (!foundEpisode)
  {
    CLog::LogF(LOGDEBUG, "Using longest playlists method");
    std::vector<unsigned int> longPlaylists;

    // Sort playlists by length
    std::vector playlists_length(playlists);
    std::ranges::sort(playlists_length,
                      [](const std::vector<unsigned int>& i, const std::vector<unsigned int>& j)
                      {
                        if (i[1] == j[1])
                          return i[0] < j[0];
                        return i[1] > j[1];
                      });

    // Remove duplicate lengths
    for (unsigned int i = 0; i < playlists_length.size() - 1; ++i)
    {
      if (playlists_length[i][1] == playlists_length[i + 1][1])
      {
        playlists_length.erase(playlists_length.begin() + (i + 1));
        --i;
      }
    }

    // Need to think about the special case of only one episode on disc
    // Firstly see how many episode length playlists there are
    // If only one, assume it's that one
    // Otherwise look for common start points (1, 800, 801)
    const unsigned int long_playlists = static_cast<unsigned int>(
        std::ranges::count_if(playlists_length, [](const std::vector<unsigned int>& x)
                              { return x[1] >= MIN_EPISODE_LENGTH; }));
    bool foundCommonPlaylist{false};
    for (const std::vector<unsigned int>& playlist : playlists)
      if (std::ranges::any_of(commonStartingPlaylists,
                              [&playlist](const unsigned int x) { return x == playlist[0]; }))
        foundCommonPlaylist = true;
    useCommonPlaylist = numEpisodes == 1 && long_playlists > numEpisodes && foundCommonPlaylist;

    // Get longest playlist(s)
    unsigned int foundPlaylists{0};
    for (const auto& playlist : playlists_length)
    {
      // Check not a 'Play All' playlist
      const auto& it = std::ranges::find_if(playAllPlaylists, [&playlist](const unsigned int& x)
                                            { return x == playlist[0]; });
      if (it == playAllPlaylists.end() && playlist[1] >= MIN_EPISODE_LENGTH &&
          (playlist.size() - PLAYLIST_CLIP_OFFSET) <= MAX_CLIPS_PER_EPISODE &&
          foundPlaylists < numEpisodes)
      {
        foundPlaylists += 1;
        longPlaylists.emplace_back(playlist[0]);
        CLog::LogF(LOGDEBUG, "Long playlist {} duration {}", playlist[0], playlist[1]);
      }
    }

    if (foundPlaylists > 0 && foundPlaylists == numEpisodes && !isSpecial && !useCommonPlaylist)
    {
      // Sort found playlists
      std::ranges::sort(longPlaylists, [](unsigned int i, unsigned int j) { return (i < j); });

      // Ensure sequential
      if (longPlaylists[0] + numEpisodes - 1 == longPlaylists[numEpisodes - 1])
      {
        for (unsigned int i = 0; i < numEpisodes; ++i)
        {
          // Now select the nth playlist for single episode
          // or numEpisodes playlists for all episodes
          if (allEpisodes || i == episodeOffset)
          {
            candidatePlaylists.emplace_back(longPlaylists[i], i);

            CLog::LogF(LOGDEBUG, "Candidate playlist {}", longPlaylists[i]);
          }
        }
        foundEpisode = true;
        findIdenticalEpisodes = true;
      }
    }

    if (isSpecial && !allEpisodes)
    {
      // Assume specials are the longest playlists that are not (assumed) episodes or play-all lists
      for (const auto& playlist : playlists_length)
      {
        // Check not a 'Play All' playlist
        const auto& it = std::ranges::find_if(playAllPlaylists, [&playlist](const unsigned int& x)
                                              { return x == playlist[0]; });

        if (it == playAllPlaylists.end() && playlist[1] >= MIN_SPECIAL_LENGTH &&
            std::ranges::count(longPlaylists, playlist[0]) == 0)
        {
          // This will only work if one special on disc (otherwise no way of knowing lengths)
          if (specialsOnDisc.size() == 1)
          {
            candidatePlaylists.emplace_back(playlist[0], 0);

            CLog::LogF(LOGDEBUG, "Candidate special playlist {}", playlist[0]);

            foundEpisode = true;
          }
        }
      }
    }
  }

  // See if we can find titles of similar length (+/- MAX_EPISODE_DIFFERENCE sec) to the desired episode or special
  // Not for all episodes
  // For single episodes use this to try and confirm/refute any episode found on length basis

  if (numEpisodes == 1 || (!foundEpisode && !allEpisodes))
  {
    if (!useCommonPlaylist)
    {
      CLog::LogF(LOGDEBUG, "Using episode length method");

      const int existingCandidates = static_cast<int>(candidatePlaylists.size());

      for (const auto& playlist : playlists)
      {
        const unsigned int titleDuration = playlist[1];
        if (episodeDuration > (titleDuration - MAX_EPISODE_DIFFERENCE) &&
            episodeDuration < (titleDuration + MAX_EPISODE_DIFFERENCE))
        {
          // episode candidate found (on basis of duration)
          candidatePlaylists.emplace_back(playlist[0], 0);

          CLog::LogF(LOGDEBUG, "Candidate playlist {} - actual duration {}, desired duration {}",
                     playlist[0], titleDuration, episodeDuration);
        }
      }

      if (!groups.empty() && !isSpecial && numEpisodes > 1)
      {
        // Found candidate groupings of playlists matching the number of episodes on disc
        // This assumes that the episodes have sequential playlist numbers

        // Firstly, cross-reference with duration based approach above
        // ie. look for episodes of correct approx duration in correct position in group of playlists
        // (titleCandidates already contains playlists of approx duration)

        CLog::LogF(LOGDEBUG, "Refining candidate titles using groups");
        for (unsigned int i = 0; i < candidatePlaylists.size(); ++i)
        {
          const unsigned int playlist = candidatePlaylists[i].first;
          bool remove{true};
          for (const auto& group : groups)
          {
            if (playlist == group.first + episodeOffset)
            {
              CLog::LogF(LOGDEBUG, "Candidate {} kept as in position {} in group", playlist,
                         episodeOffset + 1);
              remove = false;
              break;
            }
          }
          if (remove)
          {
            CLog::LogF(LOGDEBUG, "Removed candidate {} as not in position {} in group", playlist,
                       episodeOffset + 1);
            candidatePlaylists.erase(candidatePlaylists.begin() + i);
            --i;
          }
        }
      }
      else if (numEpisodes == 1 && existingCandidates > 0 && candidatePlaylists.size() > 1)
      {
        // If we have candidates from another method (ie. length) then try and decide which of them is most likely to be the single episode
        // Favour one with most languages, otherwise favour one based on matching length

        // Get languages
        std::vector<int> langs;
        langs.reserve(candidatePlaylists.size());
        for (const auto& playlist : candidatePlaylists)
          langs.emplace_back(
              static_cast<int>(std::ranges::count(playlist_langs[playlist.first], '/') + 1));

        // Loop backwards as desired length preferred to the longest length
        int keepPlaylist = -1;
        for (unsigned int i = static_cast<unsigned int>(candidatePlaylists.size()); i > 0; --i)
        {
          if (langs[i - 1] > 1)
          {
            // If more than one language then keep
            keepPlaylist = static_cast<int>(candidatePlaylists[i - 1].first);
            break;
          }
        }
        if (keepPlaylist != -1)
          // Remove other playlists (keep the first one with more than one language)
          std::erase_if(candidatePlaylists,
                        [&keepPlaylist](const std::pair<unsigned int, unsigned int>& p)
                        { return static_cast<int>(p.first) != keepPlaylist; });
        else if (existingCandidates > 0)
          // Keep those based on desired length
          candidatePlaylists.erase(candidatePlaylists.begin(),
                                   candidatePlaylists.begin() + existingCandidates);
      }
    }
    // candidatePlaylists now contains playlists of the approx duration, refined by group position (if there were groups)
    // If found nothing then it could be duration is wrong (ie from scraper)
    // In which case favour groups starting with common start points (eg. 1, 800, 801)

    if (candidatePlaylists.empty() && !isSpecial)
    {
      CLog::LogF(LOGDEBUG, "Using find playlist using candidate groups alone method");

      for (const auto& commonPlaylist : commonStartingPlaylists)
      {
        bool foundFirst{false};
        for (const auto& playlist : playlists)
        {
          if (playlist[0] == commonPlaylist)
          {
            // Need to make sure the beginning of the candidate group is present
            // Otherwise a group starting at 802 and containing 803 would be found, whereas the intention would be a group starting with 800
            // (or whatever candidateGroup is)
            CLog::LogF(LOGDEBUG, "Potential candidate group start at playlist {}", commonPlaylist);
            foundFirst = true;
          }

          const unsigned int duration = playlist[1];
          if (foundFirst && playlist[0] == commonPlaylist + episodeOffset &&
              duration >= MIN_EPISODE_LENGTH)
          {
            CLog::LogF(LOGDEBUG, "Candidate playlist {} duration {}",
                       commonPlaylist + episodeOffset, duration);
            candidatePlaylists.emplace_back(playlist[0], 0);
            findIdenticalEpisodes = true;
          }
        }
      }
    }
  }

  // candidatePlaylists should now (ideally) contain one or more candidate titles for the episode
  // Now look at durations of found playlist and add identical (in case language options)
  // Note this has already happened with the episode duration method

  if (findIdenticalEpisodes && !candidatePlaylists.empty())
  {
    const unsigned int n = static_cast<unsigned int>(
        candidatePlaylists.size()); // Save here as add potential clips to end
    for (unsigned int i = 0; i < n; ++i)
    {
      // Find candidatePlaylist duration
      const auto& it = std::ranges::find_if(playlists, [&](const std::vector<unsigned int>& x)
                                            { return x[0] == candidatePlaylists[i].first; });

      // Look for other playlists of same duration with same clips
      for (const auto& playlist : playlists)
      {
        if (candidatePlaylists[i].first != playlist[0] && it->size() == playlist.size() &&
            std::equal(it->begin() + 1, it->end(), playlist.begin() + 1))
        {
          CLog::LogF(LOGDEBUG, "Adding playlist {} as same duration as playlist {}", playlist[0],
                     candidatePlaylists[i].first);
          candidatePlaylists.emplace_back(playlist[0], candidatePlaylists[i].second);
        }
      }
    }
  }

  // Remove duplicates (ie. those that play exactly the same clip with same languages)

  if (candidatePlaylists.size() > 1)
  {
    for (unsigned int i = 0; i < candidatePlaylists.size() - 1; ++i)
    {
      const auto& it = std::ranges::find_if(playlists, [&](const std::vector<unsigned int>& x)
                                            { return x[0] == candidatePlaylists[i].first; });

      for (unsigned int j = i + 1; j < candidatePlaylists.size(); ++j)
      {
        const auto& it2 = std::ranges::find_if(playlists, [&](const std::vector<unsigned int>& x)
                                               { return x[0] == candidatePlaylists[j].first; });

        if (std::equal(it->begin() + PLAYLIST_CLIP_OFFSET, it->end(),
                       it2->begin() + PLAYLIST_CLIP_OFFSET))
        {
          // Clips are the same so check languages
          if (playlist_langs[candidatePlaylists[i].first] ==
              playlist_langs[candidatePlaylists[j].first])
          {
            // Remove duplicate
            CLog::LogF(LOGDEBUG, "Removing duplicate playlist {}", candidatePlaylists[j].first);
            candidatePlaylists.erase(candidatePlaylists.begin() + j);
            --j;
          }
        }
      }
    }
  }

  CLog::LogF(LOGDEBUG, "*** Episode Search End ***");

  // ** Now populate CFileItemList to return
  CFileItemList newItems;
  for (const auto& playlist : candidatePlaylists)
  {
    const auto newItem{std::make_shared<CFileItem>("", false)};

    // Get clips
    const auto& it = std::ranges::find_if(playlists, [&playlist](const std::vector<unsigned int>& x)
                                          { return x[0] == playlist.first; });
    const int duration = static_cast<int>(it->at(1));

    // Get languages
    std::string langs = playlist_langs[playlist.first];

    std::string buf;
    CURL path(url);
    buf = StringUtils::Format("BDMV/PLAYLIST/{:05}.mpls", playlist.first);
    path.SetFileName(buf);
    newItem->SetPath(path.Get());

    newItem->GetVideoInfoTag()->SetDuration(duration);
    newItem->GetVideoInfoTag()->m_iTrack = static_cast<int>(playlist.first);

    // Get episode title
    buf = StringUtils::Format("{0:s} {1:d} - {2:s}", g_localizeStrings.Get(20359) /* Episode */,
                              episodesOnDisc[playlist.second].m_iEpisode,
                              episodesOnDisc[playlist.second].GetTitle());
    newItem->m_strTitle = buf;
    newItem->SetLabel(buf);
    newItem->SetLabel2(StringUtils::Format(
        g_localizeStrings.Get(25005) /* Title: {0:d} */ + " - {1:s}: {2:s}\n\r{3:s}: {4:s}",
        playlist.first, g_localizeStrings.Get(180) /* Duration */,
        StringUtils::SecondsToTimeString(duration), g_localizeStrings.Get(24026) /* Languages */,
        langs));
    newItem->m_dwSize = 0;
    newItem->SetArt("icon", "DefaultVideo.png");
    items.Add(newItem);
  }
}

void CDiscDirectoryHelper::AddRootOptions(CURL url, CFileItemList& items, bool addMenuOption)
{
  CURL path(url);
  path.SetFileName(URIUtils::AddFileToFolder(url.GetFileName(), "titles"));

  auto item{std::make_shared<CFileItem>(path.Get(), true)};
  item->SetLabel(g_localizeStrings.Get(25002) /* All titles */);
  item->SetArt("icon", "DefaultVideoPlaylists.png");
  items.Add(item);

  if (addMenuOption)
  {
    path.SetFileName("menu");
    item = {std::make_shared<CFileItem>(path.Get(), false)};
    item->SetLabel(g_localizeStrings.Get(25003) /* Menu */);
    item->SetArt("icon", "DefaultProgram.png");
    items.Add(item);
  }
}

std::string CDiscDirectoryHelper::HexToString(std::span<const uint8_t> buf, int count)
{
  std::stringstream ss;
  ss << std::hex << std::setw(count) << std::setfill('0');
  std::ranges::for_each(buf, [&](auto x) { ss << static_cast<int>(x); });
  return ss.str();
}