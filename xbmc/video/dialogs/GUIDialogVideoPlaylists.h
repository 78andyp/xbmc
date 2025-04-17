/*
 *  Copyright (C) 2005-2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "dialogs/GUIDialogBoxBase.h"
#include "view/GUIViewControl.h"

#include <string>
#include <vector>

class CFileItem;
class CFileItemList;

class CGUIDialogVideoPlaylists : public CGUIDialogBoxBase
{
public:
  CGUIDialogVideoPlaylists(void);
  ~CGUIDialogVideoPlaylists(void) override;
  bool OnMessage(CGUIMessage& message) override;
  bool OnBack(int actionID) override;

  void Reset();
  int  Add(const std::string& strLabel);
  int  Add(const CFileItem& item);
  void SetItems(const CFileItemList& items);
  const CFileItemPtr GetSelectedFileItem() const;
  int GetSelectedItem() const;
  const std::vector<int>& GetSelectedItems() const;
  bool IsOKButtonPressed() const;
  bool IsChapterButtonPressed() const;
  void Sort(bool bSortOrder = true) const;
  void SetUseDetails(bool useDetails);
  void SetMultiSelection(bool multiSelection);
  void SetButtonFocus(bool buttonFocus);

protected:
  CGUIControl *GetFirstFocusableControl(int id) override;
  void OnWindowLoaded() override;
  void OnInitWindow() override;
  void OnDeinitWindow(int nextWindowID) override;
  void OnWindowUnload() override;

  virtual void OnSelect(int idx);

  CFileItemPtr m_selectedItem;
  std::unique_ptr<CFileItemList> m_vecList;
  CGUIViewControl m_viewControl;

private:
  bool m_bOKButtonPressed;
  bool m_bChapterButtonPressed;
  bool m_useDetails;
  bool m_multiSelection;
  bool m_focusToButton{};

  std::vector<int> m_selectedItems;
};
