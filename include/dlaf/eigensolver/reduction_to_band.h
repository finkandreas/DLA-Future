//
// Distributed Linear Algebra with Future (DLAF)
//
// Copyright (c) 2018-2023, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause
//

#pragma once

/// @file

#include <dlaf/common/vector.h>
#include <dlaf/communication/communicator_grid.h>
#include <dlaf/eigensolver/reduction_to_band/api.h>
#include <dlaf/matrix/matrix.h>
#include <dlaf/sender/when_all_lift.h>
#include <dlaf/util_matrix.h>

namespace dlaf::eigensolver {

namespace internal {

template <class T>
auto groupTausFromBandsToTiles(
    common::internal::vector<pika::shared_future<common::internal::vector<T>>> taus_band,
    const SizeType band_size, const SizeType mb) {
  using common::internal::vector;

  namespace ex = pika::execution::experimental;

  auto accumulateVectorSizes = [](const SizeType acc, const vector<T>& v) { return acc + v.size(); };

  vector<pika::shared_future<vector<T>>> taus;

  const SizeType group_size = mb / band_size;
  taus.reserve(dlaf::util::ceilDiv(taus_band.size(), group_size));

  for (SizeType start = 0; start < taus_band.size(); start += group_size) {
    const SizeType end = std::min<SizeType>(taus_band.size(), start + group_size);
    std::vector<pika::shared_future<vector<T>>> block_deps(begin(taus_band) + start,
                                                           begin(taus_band) + end);
    taus.emplace_back(ex::when_all_vector(std::move(block_deps)) |
                      ex::then([accumulateVectorSizes](std::vector<vector<T>>&& taus_band_chunks) {
                        const auto nrefls =
                            std::accumulate(cbegin(taus_band_chunks), cend(taus_band_chunks),
                                            SizeType(0), accumulateVectorSizes);

                        vector<T> taus_tile;
                        taus_tile.reserve(nrefls);

                        for (auto& taus_band : taus_band_chunks)
                          for (auto& tau : taus_band)
                            taus_tile.emplace_back(std::move(tau));

                        return taus_tile;
                      }) |
                      ex::make_future());
  }
  return taus;
}

}

/// Reduce a local lower Hermitian matrix to symmetric band-diagonal form, with specified band_size.
///
/// See the related distributed version for more details.
//
/// @param mat_a on entry it contains an Hermitian matrix, on exit it is overwritten with the
/// band-diagonal result together with the elementary reflectors. Just the tiles of the lower
/// triangular part will be used.
/// @param band_size size of the band of the resulting matrix (main diagonal + band_size sub-diagonals)
///
/// @pre mat_a has a square size
/// @pre mat_a has a square block size
/// @pre mat_a is a local matrix
/// @pre mat_a.blockSize().rows() % band_size == 0
template <Backend B, Device D, class T>
common::internal::vector<pika::shared_future<common::internal::vector<T>>> reductionToBand(
    Matrix<T, D>& mat_a, const SizeType band_size) {
  DLAF_ASSERT(matrix::square_size(mat_a), mat_a);
  DLAF_ASSERT(matrix::square_blocksize(mat_a), mat_a);

  DLAF_ASSERT(matrix::local_matrix(mat_a), mat_a);

  DLAF_ASSERT(band_size >= 2, band_size);
  DLAF_ASSERT(mat_a.blockSize().rows() % band_size == 0, mat_a.blockSize().rows(), band_size);

  return internal::groupTausFromBandsToTiles(internal::ReductionToBand<B, D, T>::call(mat_a, band_size),
                                             band_size, mat_a.blockSize().rows());
}

/// Reduce a distributed lower Hermitian matrix to symmetric band-diagonal form, with specified band_size.
///
/// The reduction from a lower Hermitian matrix to the band-diagonal form is performed by an orthogonal
/// similarity transformation Q, applied from left and right as in equation `Q**H . A . Q`, and whose
/// result is stored in-place in @p mat_a.
///
/// The Q matrix is a product of elementary Householder reflectors
/// `Q` = H(1) . H(2) . ... . H(n)
///
/// with `H(i) = I - tau(i) * v(i) . v(i)**H`
///
/// which are stored, together with the resulting band-diagonal matrix, in-place in the lower triangular
/// part of @p mat_a.
///
/// In particular, @p mat_a will look like this (tile representation) if band_size == blocksize
///
/** @verbatim
B ~ ~ ~ ~ ~
* B ~ ~ ~ ~
v * B ~ ~ ~
v v * B ~ ~
v v v * B ~
v v v v * B
@endverbatim
*/
///
/// where each column of `v` is an elementary reflector without its first element (which is always equal
/// to 1), `B` are the tiles containg the band-diagonal form, while `*` tiles contain both elements
/// of the band (upper triangular diagonal included) and of the elementary reflectors (lower triangular
/// diagonal excluded).
///
/// In case band_size < blocksize:
/** @verbatim
* ~ ~ ~ ~ ~
* * ~ ~ ~ ~
v * * ~ ~ ~
v v * * ~ ~
v v v * * ~
v v v v * *
@endverbatim
*/
/// @param grid is the CommunicatorGrid on which @p mat_a is distributed
/// @param mat_a on entry it contains an Hermitian matrix, on exit it is overwritten with the
/// band-diagonal result together with the elementary reflectors as described above. Just the tiles of
/// the lower triangular part will be used.
/// @param band_size size of the band of the resulting matrix (main diagonal + band_size sub-diagonals)
///
/// @pre mat_a has a square size
/// @pre mat_a has a square block size
/// @pre mat_a is distributed according to @p grid
/// @pre mat_a.blockSize().rows() % band_size == 0
template <Backend B, Device D, class T>
common::internal::vector<pika::shared_future<common::internal::vector<T>>> reductionToBand(
    comm::CommunicatorGrid grid, Matrix<T, D>& mat_a, const SizeType band_size) {
  DLAF_ASSERT(matrix::square_size(mat_a), mat_a);
  DLAF_ASSERT(matrix::square_blocksize(mat_a), mat_a);
  DLAF_ASSERT(matrix::equal_process_grid(mat_a, grid), mat_a, grid);

  DLAF_ASSERT(band_size >= 2, band_size);
  DLAF_ASSERT(mat_a.blockSize().rows() % band_size == 0, mat_a.blockSize().rows(), band_size);

  return internal::groupTausFromBandsToTiles(internal::ReductionToBand<B, D, T>::call(grid, mat_a,
                                                                                      band_size),
                                             band_size, mat_a.blockSize().rows());
}
}
