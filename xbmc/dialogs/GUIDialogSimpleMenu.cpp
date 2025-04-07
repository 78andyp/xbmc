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
  if (!selectedItem.IsFolder())
  {
    // See if already selected
    if (!usedPlaylists.empty())
    {
      // See if playlist already used
      const int newPlaylist{selectedItem.GetProperty("bluray_playlist").asInteger32(0)};
      auto matchingPlaylists{usedPlaylists |
                             std::views::filter([newPlaylist](const CVideoDatabase::PlaylistInfo& p)
                                                { return p.playlist == newPlaylist; })};

      if (std::ranges::distance(matchingPlaylists) > 0)
      {
        // Warn that this playlist is already associated with an episode
        if (!CGUIDialogYesNo::ShowAndGetInput(CVariant{559}, CVariant{25015}))
          return false;

          std::string base{item.GetDynPath()};
          if (URIUtils::IsBlurayPath(base))
            base = URIUtils::GetDiscFile(base);

        CVideoDatabase database;
        for (const auto& it : matchingPlaylists)
        {
            // Revert file to base file (BDMV/ISO)
            database.BeginTransaction();
            if (database.SetFileForMedia(base, it.mediaType, it.idMedia,
                CVideoDatabase::FileRecord{
                    .m_idFile = it.idFile,
                    .m_dateAdded = item.GetVideoInfoTag()->m_dateAdded}) >
                    0)
                database.CommitTransaction();
            else
                database.RollbackTransaction();
        }
      }
    }
  }
  return true;
}
