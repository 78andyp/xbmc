/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "filesystem/Directory.h"

#include <string>

class CFileItem;
class CFileItemList;

class CGUIDialogSimpleMenu
{
public:

  /*! \brief Show dialog allowing selection of wanted playback item */
  static bool ShowPlaySelection(CFileItem& item,
                                bool forceSelection = false,
                                const std::vector<int>* excludePlaylists = nullptr);

protected:
  static bool GetDirectoryItems(const std::string& path,
                                CFileItemList& items,
                                const XFILE::CDirectory::CHints& hints);
  static bool GetEpisodeDirectoryItems(const std::string& path,
                                       CFileItemList& items,
                                       const CFileItem& item);

private:
  static bool GetItems(const CFileItem& item,
                       CFileItemList& items,
                       const std::string& directory,
                       const std::vector<int>* excludePlaylists);
};
