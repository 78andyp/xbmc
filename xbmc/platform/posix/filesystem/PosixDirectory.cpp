/*
 *  Copyright (C) 2014-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PosixDirectory.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "URL.h"
#include "utils/AliasShortcutUtils.h"
#include "utils/CharsetConverter.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XTimeUtils.h"
#include "utils/log.h"

#include <dirent.h>
#ifdef HAVE_GETMNTENT_R
#include <mntent.h>
#endif
#include <sys/stat.h>

using namespace XFILE;

CPosixDirectory::CPosixDirectory(void) = default;

CPosixDirectory::~CPosixDirectory(void) = default;

bool CPosixDirectory::GetDirectory(const CURL& url, CFileItemList &items)
{
  std::string root = url.Get();

  if (IsAliasShortcut(root, true))
    TranslateAliasShortcut(root);

  DIR *dir = opendir(root.c_str());
  if (!dir)
    return false;

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL)
  {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    std::string itemLabel(entry->d_name);
    CCharsetConverter::unknownToUTF8(itemLabel);
    CFileItemPtr pItem(new CFileItem(itemLabel));
    std::string itemPath(URIUtils::AddFileToFolder(root, entry->d_name));

    bool bStat = false;
    struct stat buffer;

    // Unix-based readdir implementations may return an incorrect dirent.d_ino value that
    // is not equal to the (correct) stat() obtained one. In this case the file type
    // could not be determined and the value of dirent.d_type is set to DT_UNKNOWN.
    // In order to get a correct value we have to incur the cost of calling stat.
    if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK)
    {
      if (stat(itemPath.c_str(), &buffer) == 0)
        bStat = true;
    }

    if (entry->d_type == DT_DIR || (bStat && S_ISDIR(buffer.st_mode)))
    {
      pItem->SetFolder(true);
      URIUtils::AddSlashAtEnd(itemPath);
    }
    else
    {
      pItem->SetFolder(false);
    }

    if (StringUtils::StartsWith(entry->d_name, "."))
      pItem->SetProperty("file:hidden", true);

    pItem->SetPath(itemPath);

    if (!(m_flags & DIR_FLAG_NO_FILE_INFO))
    {
      if (bStat || stat(pItem->GetPath().c_str(), &buffer) == 0)
      {
        KODI::TIME::FileTime fileTime, localTime;
        KODI::TIME::TimeTToFileTime(buffer.st_mtime, &fileTime);
        KODI::TIME::FileTimeToLocalFileTime(&fileTime, &localTime);
        pItem->SetDateTime(localTime);

        if (!pItem->IsFolder())
          pItem->SetSize(buffer.st_size);
      }
    }
    items.Add(pItem);
  }
  closedir(dir);
  return true;
}

bool CPosixDirectory::Create(const CURL& url)
{
  if (!Create(url.Get()))
    return Exists(url);

  return true;
}

bool CPosixDirectory::Create(const std::string& path)
{
  if (mkdir(path.c_str(), 0755) != 0)
  {
    if (errno == ENOENT)
    {
      auto sep = path.rfind('/');
      if (sep == std::string::npos)
        return false;

      if (Create(path.substr(0, sep)))
        return Create(path);
    }

    return false;
  }
  return true;
}

bool CPosixDirectory::Remove(const CURL& url)
{
  if (rmdir(url.Get().c_str()) == 0)
    return true;

  return !Exists(url);
}

bool CPosixDirectory::RemoveRecursive(const CURL& url)
{
  std::string root = url.Get();

  if (IsAliasShortcut(root, true))
    TranslateAliasShortcut(root);

  DIR *dir = opendir(root.c_str());
  if (!dir)
    return false;

  bool success(true);
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL)
  {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    std::string itemLabel(entry->d_name);
    CCharsetConverter::unknownToUTF8(itemLabel);
    std::string itemPath(URIUtils::AddFileToFolder(root, entry->d_name));

    bool bStat = false;
    struct stat buffer;

    // Unix-based readdir implementations may return an incorrect dirent.d_ino value that
    // is not equal to the (correct) stat() obtained one. In this case the file type
    // could not be determined and the value of dirent.d_type is set to DT_UNKNOWN.
    // In order to get a correct value we have to incur the cost of calling stat.
    if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK)
    {
      if (stat(itemPath.c_str(), &buffer) == 0)
        bStat = true;
    }

    if (entry->d_type == DT_DIR || (bStat && S_ISDIR(buffer.st_mode)))
    {
      if (!RemoveRecursive(CURL{ itemPath }))
      {
        success = false;
        break;
      }
    }
    else
    {
      if (unlink(itemPath.c_str()) != 0)
      {
        success = false;
        break;
      }
    }
  }

  closedir(dir);

  if (success)
  {
    if (rmdir(root.c_str()) != 0)
      success = false;
  }

  return success;
}

bool CPosixDirectory::Exists(const CURL& url)
{
  std::string path = url.Get();

  if (IsAliasShortcut(path, true))
    TranslateAliasShortcut(path);

  struct stat buffer;
  if (stat(path.c_str(), &buffer) != 0)
    return false;
  return S_ISDIR(buffer.st_mode) ? true : false;
}

std::string CPosixDirectory::ResolveMountPoint(const std::string& file) const
{
#ifdef HAVE_GETMNTENT_R
  // Check if file is a block device, if yes we need the mount point
  struct stat st;

  if (stat(file.c_str(), &st) == 0 && S_ISBLK(st.st_mode))
  {
    // the path may be a symlink (e.g. /dev/dvd -> /dev/sr0), so make sure
    // we look up the real device
    char pathBuffer[MAX_PATH];
    char* bdDevice = realpath(file.c_str(), pathBuffer);

    if (bdDevice == nullptr)
    {
      CLog::LogF(LOGERROR, "realpath on '{}' failed with {}", file, errno);
      return file;
    }

    struct FileDeleter
    {
      void operator()(FILE* f) { endmntent(f); }
    };
    std::unique_ptr<FILE, FileDeleter> mtab(setmntent("/proc/self/mounts", "r"));

    if (mtab)
    {
      mntent *m, mbuf;
      char buf[8192];
      std::string_view bdDeviceSv = bdDevice;

      while ((m = getmntent_r(mtab.get(), &mbuf, buf, sizeof(buf))) != nullptr)
      {
        if (bdDeviceSv == m->mnt_fsname)
        {
          return m->mnt_dir;
        }
      }

      CLog::LogF(LOGINFO, "No mount point for device '{}' found", bdDeviceSv);
    }
    else
      CLog::LogF(LOGWARNING, "Failed to open mount table");
  }
#endif
  return file;
}
