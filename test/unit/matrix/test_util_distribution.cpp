//
// Distributed Linear Algebra with Future (DLAF)
//
// Copyright (c) 2018-2023, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause
//

#include <dlaf/matrix/util_distribution.h>
#include <dlaf/types.h>

#include <gtest/gtest.h>

using namespace dlaf;
using namespace dlaf::util::matrix;
using namespace testing;

struct Parameters {
  // Distribution settings
  SizeType tile_size;
  SizeType tiles_per_block;
  int rank;
  int grid_size;
  int src_rank;
  // Valid indices
  SizeType global_element;
  SizeType global_tile;
  SizeType rank_tile;
  SizeType local_tile;
  SizeType local_tile_next;
  SizeType tile_element;
};

TEST(UtilDistribution, IndexConversion) {
  std::vector<Parameters> tests = {
      // {tile_size, tiles_per_block, rank, grid_size, src_rank, global_element, global_tile,
      // rank_tile, local_tile, local_tile_next, tile_element}
      {10, 1, 0, 1, 0, 31, 3, 0, 3, 3, 1},    {10, 1, 0, 5, 0, 102, 10, 0, 2, 2, 2},
      {10, 1, 1, 5, 0, 124, 12, 2, -1, 3, 4}, {10, 1, 4, 5, 3, 124, 12, 0, -1, 3, 4},
      {25, 1, 0, 1, 0, 231, 9, 0, 9, 9, 6},   {25, 1, 0, 5, 0, 102, 4, 4, -1, 1, 2},
      {25, 1, 3, 5, 4, 102, 4, 3, 0, 0, 2},   {25, 1, 4, 5, 3, 0, 0, 3, -1, 0, 0},
      {25, 1, 0, 5, 3, 0, 0, 3, -1, 0, 0},    {25, 1, 3, 5, 3, 0, 0, 3, 0, 0, 0},

      {10, 3, 0, 1, 0, 31, 3, 0, 3, 3, 1},    {10, 2, 0, 5, 0, 102, 10, 0, 2, 2, 2},
      {10, 4, 1, 5, 0, 124, 12, 3, -1, 4, 4}, {10, 4, 4, 5, 3, 124, 12, 1, -1, 4, 4},
      {25, 5, 0, 1, 0, 231, 9, 0, 9, 9, 6},   {25, 4, 0, 5, 0, 652, 26, 1, -1, 8, 2},
      {25, 4, 1, 5, 0, 652, 26, 1, 6, 6, 2},  {25, 4, 2, 5, 0, 652, 26, 1, -1, 4, 2},
      {25, 3, 3, 5, 2, 102, 4, 3, 1, 1, 2},   {25, 3, 4, 5, 3, 0, 0, 3, -1, 0, 0},
      {25, 2, 0, 5, 3, 0, 0, 3, -1, 0, 0},    {25, 2, 3, 5, 3, 0, 0, 3, 0, 0, 0},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.global_tile, tileFromElement(test.global_element, test.tile_size));
    EXPECT_EQ(test.tile_element, tileElementFromElement(test.global_element, test.tile_size));
    EXPECT_EQ(test.global_element,
              elementFromTileAndTileElement(test.global_tile, test.tile_element, test.tile_size));
    EXPECT_EQ(test.rank_tile,
              rankGlobalTile(test.global_tile, test.tiles_per_block, test.grid_size, test.src_rank));
    EXPECT_EQ(test.local_tile, localTileFromGlobalTile(test.global_tile, test.tiles_per_block,
                                                       test.grid_size, test.rank, test.src_rank));
    EXPECT_EQ(test.local_tile_next,
              nextLocalTileFromGlobalTile(test.global_tile, test.tiles_per_block, test.grid_size,
                                          test.rank, test.src_rank));
    if (test.local_tile >= 0) {
      EXPECT_EQ(test.global_tile, globalTileFromLocalTile(test.local_tile, test.tiles_per_block,
                                                          test.grid_size, test.rank, test.src_rank));
    }
  }
}
