/*
 *  Copyright (C) 2005-2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <map>
#include <span>
#include <string>
#include <vector>

static constexpr unsigned int MIN_EPISODE_LENGTH = 10 * 60; // 10 minutes
static constexpr unsigned int MAX_EPISODE_DIFFERENCE = 30; // 30 seconds
static constexpr unsigned int MIN_SPECIAL_LENGTH = 5 * 60; // 5 minutes
static constexpr unsigned int MAX_CLIPS_PER_EPISODE = 3;
static constexpr unsigned int PLAYLIST_CLIP_OFFSET =
    2; //First two entries in playlist array are playlist number and duration. Remaining entries are clip(s)
static constexpr unsigned int CLIP_PLAYLIST_OFFSET =
    2; // First two entries in clip array are clip number and duration. Remaining entries are playlist(s)

class CFileItem;
class CFileItemList;
class CURL;
class CVideoInfoTag;

class CDiscDirectoryHelper
{
public:
  static void GetEpisodeTitles(CURL url,
                               const CFileItem& episode,
                               CFileItemList& items,
                               std::vector<CVideoInfoTag> episodesOnDisc,
                               const std::vector<std::vector<unsigned int>>& clips,
                               const std::vector<std::vector<unsigned int>>& playlists,
                               std::map<unsigned int, std::string>& playlist_langs);
  static void AddRootOptions(CURL url, CFileItemList& items, bool addMenuOption);

  static std::string HexToString(std::span<const uint8_t> buf, int count);

  static std::string GetEpisodesLabel(CFileItem& newItem, const CFileItem& item);
};