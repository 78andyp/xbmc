/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "video/VideoInfoScanner.h"

#include <gtest/gtest.h>

using namespace KODI;
using ::testing::Test;
using ::testing::WithParamInterface;
using ::testing::ValuesIn;

typedef struct episodes
{
  const char* path;
  int season;
  int episode[4]{}; // for multi-episodes
  int season2{-1};
  int episode2[4]{};
} TestEntry;

static const TestEntry TestData[] = {
    //season+episode
    {"foo.S02E03.mkv", 2, {3}},
    {"foo.203.mkv", 2, {3}},
    //episode only
    {"foo.Ep03.mkv", 1, {3}},
    {"foo.Ep_03.mkv", 1, {3}},
    {"foo.Part.III.mkv", 1, {3}},
    {"foo.Part.3.mkv", 1, {3}},
    {"foo.E03.mkv", 1, {3}},
    {"foo.2009.E03.mkv", 1, {3}},
    // multi-episode
    {"The Legend of Korra - S01E01-02 - Welcome to Republic City & A Leaf in the Wind.mkv",
     1,
     {1, 2}},
    {"foo.S01E01E02.mkv", 1, {1, 2}},
    {"foo.S01E03E04E05.mkv", 1, {3, 4, 5}},
    {"foo.S02E01-S02E02.mkv", 2, {1, 2}},
    {"foo S02E01-bar-S02E02-bar.mkv", 2, {1, 2}},
    {"foo S02E01-S02E02-S02E03.mkv", 2, {1, 2, 3}},
    {"foo 2x01-2x02.mkv", 2, {1, 2}},
    {"foo ep01-ep02.mkv", 1, {1, 2}},
    {"foo S02E01-02-03.mkv", 2, {1, 2, 3}},
    {"foo 2x01x02.mkv", 2, {1, 2}},
    {"foo ep01-02.mkv", 1, {1, 2}},
    // episode ranges
    {"foo.E03-05.mkv", 1, {3, 4, 5}},
    {"foo.ep03-05.mkv", 1, {3, 4, 5}},
    {"foo.ep03-ep05.mkv", 1, {3, 4, 5}},
    {"foo.5x03-5x05.mkv", 5, {3, 4, 5}},
    {"foo.S05E03-05.mkv", 5, {3, 4, 5}},
    {"foo.S05E03-E05.mkv", 5, {3, 4, 5}},
    {"foo.S05E03-S05E05.mkv", 5, {3, 4, 5}},
    {"foo.5x03-x05.mkv", 5, {3, 4, 5}},
    {"foo.5x03-05.mkv", 5, {3, 4, 5}},
    // should not be treated as episode ranges
    {"foo.S05E01-E03-E05.mkv", 5, {1, 3, 5}},
    {"foo.S05E01-S05E03-S05E05.mkv", 5, {1, 3, 5}},
    // multi-season
    {"foo.S00E100S05E03E04E05.mkv", 0, {100}, 5, {3, 4, 5}},
    {"foo.S01E01E02S02E01.mkv", 1, {1, 2}, 2, {1}},
    {"foo.S00E01-04S05E03-E06.mkv", 0, {1, 2, 3, 4}, 5, {3, 4, 5, 6}},
    {"foo.S00E01-E03S05E03-E05", 0, {1, 2, 3}, 5, {3, 4, 5}},
    {"foo.S05E03-05S00E01-03", 5, {3, 4, 5}, 0, {1, 2, 3}},
    // expected (partial) failure
    {"foo.S01E01-S02E03.mkv", -1}, // cannot determine number of episodes in S01
    {"foo.S01E03-S01E01.mkv", -1}, // range backwards
    {"foo.S01E03-E01.mkv", -1}, // range backwards
    {"foo.S00E01-04S01E01-S02E03.mkv", 0, {1, 2, 3, 4}}, // second range invalid
    {"foo.S01E03-S02E02S03E01-E04", 3, {1, 2, 3, 4}}}; // first range invalid

class TestVideoInfoScanner : public Test,
                             public WithParamInterface<TestEntry>
{
};

TEST_P(TestVideoInfoScanner, EnumerateEpisodeItem)
{
  const TestEntry& entry = GetParam();
  VIDEO::CVideoInfoScanner scanner;
  CFileItem item(entry.path, false);
  VIDEO::EPISODELIST expected;
  if (entry.season != -1)
  {
    for (int i = 0; i < 4 && entry.episode[i]; i++)
      expected.emplace_back(entry.season, entry.episode[i], 0, false);
    if (entry.season2 != -1)
      for (int i = 0; i < 4 && entry.episode2[i]; i++)
        expected.emplace_back(entry.season2, entry.episode2[i], 0, false);
  }

  VIDEO::EPISODELIST result;
  ASSERT_TRUE(scanner.EnumerateEpisodeItem(&item, result));
  EXPECT_EQ(expected.size(), result.size());
  if (expected.size() == result.size())
  {
    for (size_t i = 0; i < expected.size(); i++)
      EXPECT_EQ(expected[i], result[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(VideoInfoScanner, TestVideoInfoScanner, ValuesIn(TestData));

TEST(TestVideoInfoScanner, EnumerateEpisodeItemByTitle)
{
  VIDEO::CVideoInfoScanner scanner;
  CFileItem item("/foo.special.mp4", false);
  VIDEO::EPISODELIST result;
  ASSERT_TRUE(scanner.EnumerateEpisodeItem(&item, result));
  ASSERT_EQ(result.size(), 1);
  ASSERT_EQ(result[0].strTitle, "foo");
}
