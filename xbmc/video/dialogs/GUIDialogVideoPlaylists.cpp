/*
 *  Copyright (C) 2005-2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIDialogVideoPlaylists.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "guilib/GUIMessage.h"
#include "guilib/LocalizeStrings.h"
#include "input/actions/ActionIDs.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "video/VideoInfoTag.h"

#include <memory>

#define CONTROL_NUMBER_OF_ITEMS 2
#define CONTROL_SIMPLE_LIST 3
#define CONTROL_DETAILED_LIST 6
#define CONTROL_OK_BUTTON 5
#define CONTROL_CHAPTER_BUTTON 8
#define CONTROL_CANCEL_BUTTON 7

CGUIDialogVideoPlaylists::CGUIDialogVideoPlaylists()
  : CGUIDialogBoxBase(WINDOW_DIALOG_SELECT_PLAYLIST, "DialogVideoPlaylists.xml"),
    m_vecList(std::make_unique<CFileItemList>()),
    m_bOKButtonPressed(false),
    m_bChapterButtonPressed(false),
    m_useDetails(false),
    m_multiSelection(false)
{
  m_bConfirmed = false;
  m_loadType = KEEP_IN_MEMORY;
}

CGUIDialogVideoPlaylists::~CGUIDialogVideoPlaylists(void) = default;

bool CGUIDialogVideoPlaylists::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_WINDOW_DEINIT:
    {
      CGUIDialogBoxBase::OnMessage(message);

      m_useDetails = false;
      m_multiSelection = false;

      // construct selected items list
      m_selectedItems.clear();
      m_selectedItem = nullptr;
      for (int i = 0; i < m_vecList->Size(); i++)
      {
        CFileItemPtr item = m_vecList->Get(i);
        if (item->IsSelected())
        {
          m_selectedItems.push_back(i);
          if (!m_selectedItem)
            m_selectedItem = item;
        }
      }
      m_vecList->Clear();
      return true;
    }

    case GUI_MSG_WINDOW_INIT:
    {
      m_bOKButtonPressed = false;
      m_bChapterButtonPressed = false;
      m_bConfirmed = false;
      CGUIDialogBoxBase::OnMessage(message);
      return true;
    }

    case GUI_MSG_CLICKED:
    {
      int iControl = message.GetSenderId();
      if (m_viewControl.HasControl(CONTROL_SIMPLE_LIST))
      {
        int iAction = message.GetParam1();
        if (ACTION_SELECT_ITEM == iAction || ACTION_MOUSE_LEFT_CLICK == iAction)
        {
          int iSelected = m_viewControl.GetSelectedItem();
          if (iSelected >= 0 && iSelected < m_vecList->Size())
          {
            CFileItemPtr item(m_vecList->Get(iSelected));

            if (m_multiSelection)
              item->Select(!item->IsSelected());
            else
            {
              for (int i = 0; i < m_vecList->Size(); i++)
                m_vecList->Get(i)->Select(false);
              item->Select(true);
              OnSelect(iSelected);
            }

            CONTROL_ENABLE(CONTROL_OK_BUTTON);
            if (item->GetVideoInfoTag()->HasChapters() && !item->HasProperty("chapter"))
              CONTROL_ENABLE(CONTROL_CHAPTER_BUTTON);
            else
              CONTROL_DISABLE(CONTROL_CHAPTER_BUTTON);
          }
        }
      }
      if (iControl == CONTROL_CHAPTER_BUTTON)
      {
        m_bChapterButtonPressed = true;
        Close();
      }
      if (iControl == CONTROL_OK_BUTTON)
      {
        m_bOKButtonPressed = true;
        Close();
      }
      else if (iControl == CONTROL_CANCEL_BUTTON)
      {
        m_selectedItem = nullptr;
        m_vecList->Clear();
        m_selectedItems.clear();
        m_bConfirmed = false;
        Close();
      }
    }
    break;

    case GUI_MSG_SETFOCUS:
    {
      if (m_viewControl.HasControl(message.GetControlId()))
      {
        if (m_vecList->IsEmpty())
        {
          SET_CONTROL_FOCUS(CONTROL_OK_BUTTON, 0);
          return true;
        }
        if (m_viewControl.GetCurrentControl() != message.GetControlId())
        {
          m_viewControl.SetFocused();
          return true;
        }
      }
    }
    break;

    default:
      break;
  }

  return CGUIDialogBoxBase::OnMessage(message);
}

void CGUIDialogVideoPlaylists::OnSelect(int idx)
{
  m_bConfirmed = true;
}

bool CGUIDialogVideoPlaylists::OnBack(int actionID)
{
  m_selectedItem = nullptr;
  m_vecList->Clear();
  m_selectedItems.clear();
  m_bConfirmed = false;
  return CGUIDialogBoxBase::OnBack(actionID);
}

void CGUIDialogVideoPlaylists::Reset()
{
  m_bOKButtonPressed = false;
  m_bChapterButtonPressed = false;

  m_useDetails = false;
  m_multiSelection = false;
  m_focusToButton = false;
  m_selectedItem = nullptr;
  m_vecList->Clear();
  m_selectedItems.clear();
}

int CGUIDialogVideoPlaylists::Add(const std::string& strLabel)
{
  CFileItemPtr pItem(new CFileItem(strLabel));
  m_vecList->Add(pItem);
  return m_vecList->Size() - 1;
}

int CGUIDialogVideoPlaylists::Add(const CFileItem& item)
{
  m_vecList->Add(std::make_shared<CFileItem>(item));
  return m_vecList->Size() - 1;
}

void CGUIDialogVideoPlaylists::SetItems(const CFileItemList& items)
{
  m_vecList->Clear();
  m_vecList->Append(items);

  m_viewControl.SetItems(*m_vecList);
}

int CGUIDialogVideoPlaylists::GetSelectedItem() const
{
  return !m_selectedItems.empty() ? m_selectedItems[0] : -1;
}

const CFileItemPtr CGUIDialogVideoPlaylists::GetSelectedFileItem() const
{
  if (m_selectedItem)
    return m_selectedItem;
  return std::make_shared<CFileItem>();
}

const std::vector<int>& CGUIDialogVideoPlaylists::GetSelectedItems() const
{
  return m_selectedItems;
}

bool CGUIDialogVideoPlaylists::IsOKButtonPressed() const
{
  return m_bOKButtonPressed;
}

bool CGUIDialogVideoPlaylists::IsChapterButtonPressed() const
{
  return m_bChapterButtonPressed;
}

void CGUIDialogVideoPlaylists::Sort(bool bSortOrder /*=true*/) const
{
  m_vecList->Sort(SortByLabel, bSortOrder ? SortOrderAscending : SortOrderDescending);
}

void CGUIDialogVideoPlaylists::SetUseDetails(bool useDetails)
{
  m_useDetails = useDetails;
}

void CGUIDialogVideoPlaylists::SetMultiSelection(bool multiSelection)
{
  m_multiSelection = multiSelection;
}

void CGUIDialogVideoPlaylists::SetButtonFocus(bool buttonFocus)
{
  m_focusToButton = buttonFocus;
}

CGUIControl* CGUIDialogVideoPlaylists::GetFirstFocusableControl(int id)
{
  if (m_viewControl.HasControl(id))
    id = m_viewControl.GetCurrentControl();
  return CGUIDialogBoxBase::GetFirstFocusableControl(id);
}

void CGUIDialogVideoPlaylists::OnWindowLoaded()
{
  CGUIDialogBoxBase::OnWindowLoaded();
  m_viewControl.Reset();
  m_viewControl.SetParentWindow(GetID());
  m_viewControl.AddView(GetControl(CONTROL_SIMPLE_LIST));
  m_viewControl.AddView(GetControl(CONTROL_DETAILED_LIST));
}

void CGUIDialogVideoPlaylists::OnInitWindow()
{
  m_viewControl.SetItems(*m_vecList);
  m_selectedItems.clear();
  for (int i = 0; i < m_vecList->Size(); i++)
  {
    auto item = m_vecList->Get(i);
    if (item->IsSelected())
    {
      m_selectedItems.push_back(i);
      if (m_selectedItem == nullptr)
        m_selectedItem = item;
    }
  }
  m_viewControl.SetCurrentView(m_useDetails ? CONTROL_DETAILED_LIST : CONTROL_SIMPLE_LIST);

  SET_CONTROL_LABEL(CONTROL_NUMBER_OF_ITEMS,
                    StringUtils::Format("{} {}", m_vecList->Size(), g_localizeStrings.Get(127)));

  SET_CONTROL_LABEL(CONTROL_OK_BUTTON, g_localizeStrings.Get(186)); // OK
  SET_CONTROL_VISIBLE(CONTROL_OK_BUTTON);
  CONTROL_DISABLE(CONTROL_OK_BUTTON);

  SET_CONTROL_LABEL(CONTROL_CHAPTER_BUTTON, g_localizeStrings.Get(21398)); // Chapters
  SET_CONTROL_VISIBLE(CONTROL_CHAPTER_BUTTON);
  CONTROL_DISABLE(CONTROL_CHAPTER_BUTTON);

  SET_CONTROL_LABEL(CONTROL_CANCEL_BUTTON, g_localizeStrings.Get(222));

  CGUIDialogBoxBase::OnInitWindow();

  // focus one of the buttons if explicitly requested
  // ATTENTION: this must be done after calling CGUIDialogBoxBase::OnInitWindow()
  if (m_focusToButton)
  {
    SET_CONTROL_FOCUS(CONTROL_OK_BUTTON, 0);
  }

  // if nothing is selected, select first item
  m_viewControl.SetSelectedItem(std::max(GetSelectedItem(), 0));
}

void CGUIDialogVideoPlaylists::OnDeinitWindow(int nextWindowID)
{
  m_viewControl.Clear();
  CGUIDialogBoxBase::OnDeinitWindow(nextWindowID);
}

void CGUIDialogVideoPlaylists::OnWindowUnload()
{
  CGUIDialogBoxBase::OnWindowUnload();
  m_viewControl.Reset();
}
