/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "IDirectory.h"
#include "URL.h"

using namespace XFILE;

/*!
 \brief Abstracts a DVD virtual directory (dvd://) which in turn points to the actual physical drive
 */
class CDVDDirectory : public IDirectory
{
public:
  CDVDDirectory() = default;
  ~CDVDDirectory() override = default;
  bool GetDirectory(const CURL& url, CFileItemList& items) override;

private:
  void GetRoot(CFileItemList& items) const;
  void GetRoot(CFileItemList& items,
               const CFileItem& episode,
               const std::vector<CVideoInfoTag>& episodesOnDisc) const;
  bool GetEpisodeDirectory(const CURL& url,
                           const CFileItem& episode,
                           CFileItemList& items,
                           const std::vector<CVideoInfoTag>& episodesOnDisc) override;
  void GetTitles(int job, CFileItemList& items, int sort) const;
  std::shared_ptr<CFileItem> GetTitle(const PlaylistVectorEntry& title,
                                      const std::string& label) const;

  CURL m_url;
};
