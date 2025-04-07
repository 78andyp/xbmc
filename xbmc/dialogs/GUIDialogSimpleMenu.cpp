/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIDialogSimpleMenu.h"

#include "FileItem.h"
#include "GUIDialogSelect.h"
#include "GUIDialogYesNo.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <memory>
#include <ranges>
#include <vector>

using namespace KODI;

bool CGUIDialogSimpleMenu::ShowPlaylistSelection(
    const CFileItem& item,
    CFileItem& selectedItem,
    const CFileItemList& items,
    const std::vector<CVideoDatabase::PlaylistInfo>& usedPlaylists)
{
  CGUIDialogSelect* dialog{CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(
      WINDOW_DIALOG_SELECT)};

  dialog->Reset();
  dialog->SetHeading(CVariant{25006}); // Select playback item
  dialog->SetItems(items);
  dialog->SetUseDetails(true);
  dialog->Open();

  selectedItem = *dialog->GetSelectedFileItem();
  if (dialog->GetSelectedItem() < 0)
  {
    CLog::LogF(LOGDEBUG, "User aborted playlist selection");
    return false;
  }

  // If item is not folder (ie. all titles)
  if (!selectedItem.m_bIsFolder)
  {
    // See if already selected
    if (!usedPlaylists.empty())
    {
      const auto& it{std::ranges::find_if(
          usedPlaylists, [&selectedItem](const CVideoDatabase::PlaylistInfo& p)
          { return p.playlist == selectedItem.GetVideoInfoTag()->m_iTrack; })};
      if (it != usedPlaylists.end())
      {
        // Warn that this playlist is already associated with an episode
        if (CGUIDialogYesNo::ShowAndGetInput(CVariant{559}, CVariant{25015}))
        {
          // Delink
          CVideoDatabase database;
          if (!database.Open())
          {
            CLog::LogF(LOGERROR, "Failed to open video database");
            return false;
          }

          // Revert file to base file (BDMV/ISO)
          const std::string base{URIUtils::GetBlurayFile(item.GetDynPath())};
          database.BeginTransaction();
          if (database.SetFileForMedia(base, VideoDbContentType::EPISODES, it->idMedia, it->idFile))
            database.CommitTransaction();
          else
            database.RollbackTransaction();
        }
        else
          return false;
      }
    }
  }
  return true;
}
