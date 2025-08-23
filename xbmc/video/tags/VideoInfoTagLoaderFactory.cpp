/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoInfoTagLoaderFactory.h"

#include "FileItem.h"
#include "VideoTagLoaderFFmpeg.h"
#include "VideoTagLoaderNFO.h"
#include "VideoTagLoaderPlugin.h"
#include "video/tags/VideoTagExtractionHelper.h"

namespace KODI::VIDEO
{

std::unique_ptr<IVideoInfoTagLoader> CVideoInfoTagLoaderFactory::CreateLoader(
    const CFileItem& item, const ADDON::ScraperPtr& info, bool lookInFolder, bool forceRefresh)
{
  // Try plugin loader first if conditions are met
  if (item.IsPlugin() && info && info->ID() == "metadata.local")
  {
    auto plugin = std::make_unique<CVideoTagLoaderPlugin>(item, forceRefresh);
    if (plugin->HasInfo())
      return plugin;
  }

  // Try NFO loader
  auto nfo = std::make_unique<CVideoTagLoaderNFO>(item, info, lookInFolder);
  if (nfo->HasInfo())
    return nfo;

  // Try FFmpeg loader if extraction is supported
  if (TAGS::CVideoTagExtractionHelper::IsExtractionSupportedFor(item))
  {
    auto ffmpeg = std::make_unique<CVideoTagLoaderFFmpeg>(item, info, lookInFolder);
    if (ffmpeg->HasInfo())
      return ffmpeg;
  }

  // No suitable loader found
  return nullptr;
}

} // namespace KODI::VIDEO
