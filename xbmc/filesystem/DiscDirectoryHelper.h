/*
 *  Copyright (C) 2005-2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "Directory.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class CFileItem;
class CFileItemList;
class CURL;
class CVideoInfoTag;

struct ChapterInfo;

namespace XFILE
{

enum class GetTitles : uint8_t
{
  GET_TITLES_ONE = 0,
  GET_TITLES_MAIN,
  GET_TITLES_ALL
};

enum class SortTitles : uint8_t
{
  SORT_TITLES_NONE = 0,
  SORT_TITLES_EPISODE,
  SORT_TITLES_MOVIE
};

enum class AddMenuOption : bool
{
  NO_MENU,
  ADD_MENU
};

enum class ENCODING_TYPE : uint8_t
{
  VIDEO_MPEG1 = 0x01,
  VIDEO_MPEG2 = 0x02,
  AUDIO_MPEG1 = 0x03,
  AUDIO_MPEG2 = 0x04,
  AUDIO_LPCM = 0x80,
  AUDIO_AC3 = 0x81,
  AUDIO_DTS = 0x82,
  AUDIO_TRUHD = 0x83,
  AUDIO_AC3PLUS = 0x84,
  AUDIO_DTSHD = 0x85,
  AUDIO_DTSHD_MASTER = 0x86,
  VIDEO_VC1 = 0xea,
  VIDEO_H264 = 0x1b,
  VIDEO_HEVC = 0x24,
  SUB_PG = 0x90,
  SUB_IG = 0x91,
  SUB_TEXT = 0x92,
  AUDIO_AC3PLUS_SECONDARY = 0xa1,
  AUDIO_DTSHD_SECONDARY = 0xa2
};

enum class ASPECT_RATIO : uint8_t
{
  RATIO_4_3 = 2,
  RATIO_16_9 = 3
};

struct DiscStreamInfo
{
  bool operator==(const DiscStreamInfo&) const = default;

  ENCODING_TYPE coding{0};
  unsigned int format{0};
  unsigned int rate{0};
  ASPECT_RATIO aspect{0};
  std::string lang;
};

struct PlaylistInfo
{
  unsigned int playlist{0};
  std::chrono::milliseconds duration{0};
  std::vector<unsigned int> clips;
  std::map<unsigned int, std::chrono::milliseconds> clipDuration;
  std::vector<std::chrono::milliseconds> chapters;
  std::vector<DiscStreamInfo> videoStreams;
  std::vector<DiscStreamInfo> audioStreams;
  std::vector<DiscStreamInfo> pgStreams;
  std::string languages;
};

struct ClipInfo
{
  std::chrono::milliseconds duration{0};
  std::vector<unsigned int> playlists;
};

using PlaylistMap = std::map<unsigned int, PlaylistInfo>;
using ClipMap = std::map<unsigned int, ClipInfo>;

static constexpr std::chrono::milliseconds MIN_EPISODE_DURATION{10 * 60 * 1000}; // 10 minutes
static constexpr std::chrono::milliseconds MAX_EPISODE_DIFFERENCE{30 * 1000}; // 30 seconds
static constexpr std::chrono::milliseconds MIN_SPECIAL_DURATION{5 * 60 * 1000}; // 5 minutes
static constexpr unsigned int MAIN_TITLE_LENGTH_PERCENT{70};

class CDiscDirectoryHelper
{
  enum class IsSpecial : bool
  {
    SPECIAL,
    EPISODE
  };

  enum class AllEpisodes : bool
  {
    SINGLE,
    ALL
  };

  struct CandidatePlaylistsDurationInformation
  {
    unsigned int playlist{0};
    std::chrono::milliseconds durationDelta{0};
    unsigned int chapters{0};
  };

  struct SortedPlaylistsInformation
  {
    unsigned int playlist{0};
    unsigned int index{0};
    std::string languages;
  };

public:
  CDiscDirectoryHelper();

  /*!
   * \brief Populates a CFileItemList with the playlist(s) corresponding to the given episode.
   * \param url bluray:// episode url
   * \param items CFileItemList to populate
   * \param episodeIndex index into episodesOnDisc
   * \param episodesOnDisc vector array of CVideoInfoTags - one for each episode on the disc (populated by GetEpisodesOnDisc)
   * \param clips map of clips on disc (populated in CBlurayDirectory)
   * \param playlists map of playlists on disc (populated in CBlurayDirectory)
   * \return true if at least one playlist is found, otherwise false
   */
  bool GetEpisodePlaylists(const CURL& url,
                           CFileItemList& items,
                           int episodeIndex,
                           const std::vector<CVideoInfoTag>& episodesOnDisc,
                           const ClipMap& clips,
                           const PlaylistMap& playlists);

  /*!
   * \brief Populates a vector array of CVideoInfoTags with information for the episodes on a bluray disc
   * \param url bluray:// episode url
   * \return vector array of CVideoInfoTags containing episode information
   */
  static std::vector<CVideoInfoTag> GetEpisodesOnDisc(const CURL& url);

  /*!
   * \brief Add All Titles and, if appropriate, Menu options to the CFileItemList
   * \param url bluray:// episode url
   * \param items CFileItemList to populate
   * \param addMenuOption Bluray disc has menu, so add Menu Option
   */
  static void AddRootOptions(const CURL& url, CFileItemList& items, AddMenuOption addMenuOption);

  /*!
   * \brief Either shows simple menu to select playlist, chooses main feature (movie/episode) playlists or returns if disc menu will be used later.
   * \param item FileItem containing details of desired movie/episode. This is updated with the selected playlist.
   * \return true if a playlist was selected or if the disc menu will be used later, false if the user cancelled.
   */
  static bool GetOrShowPlaylistSelection(CFileItem& item);

  static std::vector<ChapterInfo> GetChapterInfo(const PlaylistInfo& playlist);

protected:
  static bool GetDirectoryItems(const std::string& path,
                                CFileItemList& items,
                                const CDirectory::CHints& hints);

private:
  void InitialisePlaylistSearch(int episodeIndex, const std::vector<CVideoInfoTag>& episodesOnDisc);
  void FindPlayAllPlaylists(const ClipMap& clips, const PlaylistMap& playlists);
  void FindGroups(const PlaylistMap& playlists);
  void FindCandidatePlaylists(const std::vector<CVideoInfoTag>& episodesOnDisc,
                              unsigned int episodeIndex,
                              const PlaylistMap& playlists);
  void FindSpecials(const PlaylistMap& playlists);
  static void GenerateItem(const CURL& url,
                           const std::shared_ptr<CFileItem>& item,
                           unsigned int playlist,
                           const PlaylistMap& playlists,
                           const CVideoInfoTag& tag,
                           IsSpecial isSpecial);
  void EndPlaylistSearch();
  void PopulateFileItems(const CURL& url,
                         CFileItemList& items,
                         int episodeIndex,
                         const std::vector<CVideoInfoTag>& episodesOnDisc,
                         const PlaylistMap& playlists);

  static bool GetItems(CFileItemList& items, const std::string& directory);

  AllEpisodes m_allEpisodes;
  IsSpecial m_isSpecial;
  unsigned int m_numEpisodes;
  unsigned int m_numSpecials;

  std::vector<unsigned int> m_playAllPlaylists;
  std::map<unsigned int, std::map<unsigned int, std::vector<unsigned int>>> m_playAllPlaylistsMap;
  std::vector<std::vector<unsigned int>> m_groups;
  std::map<unsigned int, unsigned int> m_candidatePlaylists;
  std::vector<unsigned int> m_candidateSpecials;
};
} // namespace XFILE
