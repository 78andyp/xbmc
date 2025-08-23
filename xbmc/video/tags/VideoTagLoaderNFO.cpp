/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoTagLoaderNFO.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "NfoFile.h"
#include "URL.h"
#include "filesystem/Directory.h"
#include "filesystem/StackDirectory.h"
#include "utils/URIUtils.h"
#include "utils/log.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoTag.h"

#include <ranges>
#include <utility>

using namespace XFILE;

CVideoTagLoaderNFO::CVideoTagLoaderNFO(const CFileItem& item,
                                       ADDON::ScraperPtr info,
                                       bool lookInFolder)
  : IVideoInfoTagLoader(item, std::move(info), lookInFolder)
{
  m_paths = FindNFO(m_item, lookInFolder);
}

bool CVideoTagLoaderNFO::HasInfo() const
{
  return !m_paths.empty();
}

CInfoScanner::InfoType CVideoTagLoaderNFO::Load(CVideoInfoTag& tag,
                                                bool prioritise,
                                                std::vector<EmbeddedArt>* art)
{
  KODI::VIDEO::NFOInformationVector nfoInformation;
  LoadMultiple(nfoInformation, prioritise);
  if (nfoInformation.size()==1)
  {
    tag = *nfoInformation[0].tag;
    return nfoInformation[0].type;
  }
  return CInfoScanner::InfoType::NONE;
}

namespace
{
std::string GetNFOTypeString(CInfoScanner::InfoType result)
{
  using enum CInfoScanner::InfoType;
  switch (result)
  {
    case COMBINED:
      return "mixed";
    case FULL:
      return "full";
    case URL:
      return "URL";
    case NONE:
      return "";
    case OVERRIDE:
      return "override";
    case ERROR_NFO:
    case TITLE:
    default:
      return "malformed";
  }
}
}

CInfoScanner::InfoType CVideoTagLoaderNFO::LoadMultiple(KODI::VIDEO::NFOInformationVector& nfoInformation, bool prioritise)
{
  for (const auto& nfoPath : m_paths)
  {
    CNfoFile nfoReader;
    CInfoScanner::InfoType result = CInfoScanner::InfoType::NONE;
    if (m_info)
      result = nfoReader.Create(nfoPath, m_info);

    auto tag = std::make_unique<CVideoInfoTag>();
    if (result == CInfoScanner::InfoType::FULL || result == CInfoScanner::InfoType::COMBINED ||
        result == CInfoScanner::InfoType::OVERRIDE)
      nfoReader.GetDetails(*tag, nullptr, prioritise);

    if (result == CInfoScanner::InfoType::URL || result == CInfoScanner::InfoType::COMBINED)
    {
      m_url = nfoReader.ScraperUrl();
      m_info = nfoReader.GetScraperInfo();
    }

    const std::string type{GetNFOTypeString(result)};
    if (!type.empty())
      CLog::Log(LOGDEBUG, "VideoInfoScanner: Found matching {} NFO file: {}", type,
                CURL::GetRedacted(nfoPath));
    nfoInformation.emplace_back(KODI::VIDEO::NFOInformation{.tag = std::move(tag), .type = result});
  }
  if (nfoInformation.empty())
    CLog::Log(LOGDEBUG, "VideoInfoScanner: No NFO file found. Using title search for '{}'",
              CURL::GetRedacted(m_item.GetPath()));

  return CInfoScanner::InfoType::NONE;
}

std::vector<std::string> CVideoTagLoaderNFO::FindNFO(const CFileItem& item, bool movieFolder) const
{
  // See if we need to resolve path to a (base) directory
  std::string path;
  std::string name;
  std::vector<std::string> foundNfos;
  if (!item.IsFolder())
  {
    path = URIUtils::GetBasePath(item.GetPath());
    name = URIUtils::GetFileName(item.GetPath());
    URIUtils::RemoveExtension(name);
  }
  else
  {
    path = item.GetPath();
    name = path;
    URIUtils::RemoveSlashAtEnd(name);
    name = URIUtils::GetFileName(name);
  }

  // Search for nfo file(s)
  // If we are looking for a specific episode nfo the file name must end with SxxEyy
  // (otherwise it could match the wrong episode nfo)
  CFileItemList items;
  if (CDirectory::GetDirectory(path, items, ".nfo", DIR_FLAG_DEFAULTS) && !items.IsEmpty())
  {
    const CVideoInfoTag* tag{item.GetVideoInfoTag()};
    auto nfoItems{items |
                  std::views::filter(
                      [&tag](const auto& nfoItem)
                      {
                        if (!nfoItem->IsNFO())
                          return false;

                        if (tag && tag->m_iSeason >= 0 && tag->m_iEpisode >= 0)
                        {
                          std::string path{nfoItem->GetPath()};
                          const std::string extension{URIUtils::GetExtension(path)};
                          if (!extension.empty())
                            path.erase(path.size() - extension.size());
                          return path.ends_with(
                              fmt::format("S{:02}E{:02}", tag->m_iSeason, tag->m_iEpisode));
                        }

                        return true;
                      }) |
                  std::views::transform([](const auto& nfoItem) { return nfoItem->GetPath(); })};

    int nfoCount{static_cast<int>(std::ranges::distance(nfoItems))};
    if (item.GetVideoContentType() == VideoDbContentType::MOVIES)
    {
      // May be multiple nfos if multi-movie media file (currently Bluray/DVD)
      if (nfoCount > 1)
      {
        // Format should be movie_name-descriptor.nfo
        for (const auto& nfo : nfoItems)
          if (nfo.starts_with(name + '-'))
            foundNfos.emplace_back(nfo);
      }
      else if (nfoCount == 1)
      {
        // Single nfo - must be either movie_name.nfo or 'movie.nfo'
        const std::string& nfo{*nfoItems.begin()};
        if (nfo.starts_with(name + '.') || nfo == "movie.nfo")
          foundNfos.emplace_back(nfo);
      }
    }
    else if (item.GetVideoContentType() == VideoDbContentType::TVSHOWS)
    {
      // Should only be one nfo
      if (nfoCount == 1)
      {
        // If specified season and episode then must be exact match (determined in filter above)
        if (tag && tag->m_iSeason >= 0 && tag->m_iEpisode >= 0)
          foundNfos.emplace_back(*nfoItems.begin());
        else
        {
          // Must be either tvshow_name.nfo or 'tvshow.nfo'
          const std::string& nfo{*nfoItems.begin()};
          if (nfo.starts_with(name + '.') || nfo == "tvshow.nfo")
            foundNfos.emplace_back(nfo);
        }
      }
      else if (item.GetVideoContentType() == VideoDbContentType::MUSICVIDEOS)
      {
        // Must be either musivcideo_name.nfo
        const std::string& nfo{*nfoItems.begin()};
        if (nfo.starts_with(name + '.'))
          foundNfos.emplace_back(nfo);
      }
    }
  }

  return foundNfos;
}
