/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DVDDirectory.h"

#include "DiscDirectoryHelper.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "cores/VideoPlayer/DVDInputStreams/DVDInputStreamNavigator.h"
#include "filesystem/Directory.h"
#include "guilib/LocalizeStrings.h"
#include "storage/MediaManager.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

using namespace XFILE;

std::shared_ptr<CFileItem> CDVDDirectory::GetTitle(const PlaylistVectorEntry& title,
                                                   const std::string& label) const
{
  const std::shared_ptr<CFileItem> item(new CFileItem("", false));
  CURL path(m_url);
  const unsigned int playlist{title.first};
  std::string buf{StringUtils::Format("title/{}", playlist)};
  path.SetFileName(buf);
  item->SetPath(path.Get());

  const int duration = static_cast<int>(title.second.duration / 90000);
  item->GetVideoInfoTag()->SetDuration(duration);
  item->GetVideoInfoTag()->m_iTrack = playlist;

  buf = StringUtils::Format(label, playlist);
  item->m_strTitle = buf;
  item->SetLabel(buf);

  buf = StringUtils::Format(g_localizeStrings.Get(25007), title.second.clips.size(),
                            StringUtils::SecondsToTimeString(duration));
  item->SetLabel2(buf);
  item->m_dwSize = 0;
  item->SetArt("icon", "DefaultVideo.png");

  return item;
}

void CDVDDirectory::GetTitles(const int job, CFileItemList& items, const int sort) const
{
  const CFileItem item{m_url, false};
  CDVDInputStreamNavigator dvd{nullptr, item};
  if (dvd.Open())
  {
    // Get playlist information
    ClipMap clips;
    PlaylistMap playlists;
    dvd.GetPlaylistInfo(clips, playlists);

    // Remove zero length titles
    std::erase_if(playlists, [](const PlaylistMapEntry& i) { return i.second.duration == 0; });

    // Get the longest title and calculate minimum title length
    unsigned int minDuration{0};
    unsigned int maxPlaylist{0};
    if (job != GET_TITLES_ALL)
    {
      for (const auto& [playlist, playlistInformation] : playlists)
        if (playlistInformation.duration > minDuration)
        {
          minDuration = playlistInformation.duration;
          maxPlaylist = playlist;
        }
    }
    minDuration = minDuration * MAIN_TITLE_LENGTH_PERCENT / 100;

    // Remove playlists shorter than minimum duration
    if (job == GET_TITLES_MAIN)
      std::erase_if(playlists, [&minDuration](const PlaylistMapEntry& i)
                    { return i.second.duration < minDuration; });
    else if (job == GET_TITLES_ONE)
      std::erase_if(playlists,
                    [&maxPlaylist](const PlaylistMapEntry& i) { return i.first != maxPlaylist; });

    PlaylistVector playlists_length;
    playlists_length.reserve(playlists.size());
    playlists_length.assign(playlists.begin(), playlists.end());
    if (sort != SORT_TITLES_NONE && playlists_length.size()>1)
    {
      std::ranges::sort(playlists_length,
                        [&sort](const PlaylistVectorEntry& i, const PlaylistVectorEntry& j)
                        {
                          const auto& [i_playlist, i_playlistInformation] = i;
                          const auto& [j_playlist, j_playlistInformation] = j;
                          if (sort == SORT_TITLES_MOVIE)
                          {
                            if (i_playlistInformation.duration == j_playlistInformation.duration)
                              return i_playlist < j_playlist;
                            return i_playlistInformation.duration > j_playlistInformation.duration;
                          }
                          return i_playlist < j_playlist;
                        });

      const auto& pivot = std::ranges::find_if(playlists_length,
                                               [&maxPlaylist](const PlaylistVectorEntry& p)
                                               {
                                                 const auto& [playlist, playlistInformation] = p;
                                                 return playlist == maxPlaylist;
                                               });
      if (pivot != playlists_length.end())
        std::rotate(playlists_length.begin(), pivot, pivot + 1);
    }

    for (const auto& title : playlists_length)
      items.Add(GetTitle(title, title.first == maxPlaylist
                                    ? g_localizeStrings.Get(25004) /* Main Title */
                                    : g_localizeStrings.Get(25005) /* Title */));

    dvd.Close();
  }
}

void CDVDDirectory::GetRoot(CFileItemList& items) const
{
  GetTitles(GET_TITLES_MAIN, items, SORT_TITLES_MOVIE);

  CDiscDirectoryHelper::AddRootOptions(m_url, items, true);
}

void CDVDDirectory::GetRoot(CFileItemList& items,
                            const CFileItem& episode,
                            const std::vector<CVideoInfoTag>& episodesOnDisc) const
{
  if (CDVDInputStreamNavigator dvd{nullptr, episode}; dvd.Open())
  {
    // Get playlist, clip and language information
    ClipMap clips;
    PlaylistMap playlists;

    dvd.GetPlaylistInfo(clips, playlists);

    // Get episode playlists
    CDiscDirectoryHelper::GetEpisodeTitles(m_url, episode, items, episodesOnDisc, clips, playlists);

    if (!items.IsEmpty())
      CDiscDirectoryHelper::AddRootOptions(m_url, items, true);

    dvd.Close();
  }
}

bool CDVDDirectory::GetDirectory(const CURL& url, CFileItemList& items)
{
  m_url = url;
  std::string file = m_url.GetFileName();
  URIUtils::RemoveSlashAtEnd(file);

  if (file == "root")
    GetRoot(items);
  else if (file == "root/titles")
    GetTitles(GET_TITLES_ALL, items, SORT_TITLES_MOVIE);
  else
  {
    const CURL url2{CURL(URIUtils::GetDiscUnderlyingFile(url))};
    CDirectory::CHints hints;
    hints.flags = m_flags;
    if (!CDirectory::GetDirectory(url2, items, hints))
      return false;

    // Found items will have underlying protocol (eg. udf:// or smb://)
    // in path so add back dvd://
    // (so properly recognised in cache as bluray:// files for CFile:Exists() etc..)
    CURL url3{url};
    for (const auto& item : items)
    {
      const CURL url4{item->GetPath()};
      url3.SetFileName(url4.GetFileName());
      item->SetPath(url3.Get());
    }

    url3.SetFileName("menu");
    const std::shared_ptr<CFileItem> item{std::make_shared<CFileItem>()};
    item->SetPath(url3.Get());
    items.Add(item);
  }

  items.AddSortMethod(SortByTrackNumber, 554,
                      LABEL_MASKS("%L", "%D", "%L", "")); // FileName, Duration | Foldername, empty
  items.AddSortMethod(SortBySize, 553,
                      LABEL_MASKS("%L", "%I", "%L", "%I")); // FileName, Size | Foldername, Size

  return true;
}

bool CDVDDirectory::GetEpisodeDirectory(const CURL& url,
                                        const CFileItem& episode,
                                        CFileItemList& items,
                                        const std::vector<CVideoInfoTag>& episodesOnDisc)
{
  m_url = url;
  GetRoot(items, episode, episodesOnDisc);

  items.AddSortMethod(SortByTrackNumber, 554,
                      LABEL_MASKS("%L", "%D", "%L", "")); // FileName, Duration | Foldername, empty
  items.AddSortMethod(SortBySize, 553,
                      LABEL_MASKS("%L", "%I", "%L", "%I")); // FileName, Size | Foldername, Size

  return true;
}