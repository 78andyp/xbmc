/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "BlurayFile.h"

#include "URL.h"
#include "utils/URIUtils.h"

#include <assert.h>

namespace XFILE
{

  CBlurayFile::CBlurayFile(void)
    : COverrideFile(false)
  { }

  CBlurayFile::~CBlurayFile(void) = default;

  std::string CBlurayFile::TranslatePath(const CURL& url)
  {
    assert(url.IsProtocol("bluray"));
    return URIUtils::GetDiscUnderlyingFile(url);
  }
} /* namespace XFILE */
