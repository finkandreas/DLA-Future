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

#include <exception>
#include <vector>

#include <pika/execution.hpp>

#include <dlaf/common/range2d.h>
#include <dlaf/communication/communicator_grid.h>
#include <dlaf/matrix/distribution.h>
#include <dlaf/matrix/internal/tile_pipeline.h>
#include <dlaf/matrix/layout_info.h>
#include <dlaf/matrix/matrix_base.h>
#include <dlaf/matrix/tile.h>
#include <dlaf/types.h>

namespace dlaf {
namespace matrix {

namespace internal {

/// Helper function returning a vector with the results of calling a function over a IterableRange2D
///
/// @param f non-void function accepting LocalTileIndex as parameter
template <class Func>
auto selectGeneric(Func&& f, common::IterableRange2D<SizeType, LocalTile_TAG> range) {
  using RetT = decltype(f(LocalTileIndex{}));

  std::vector<RetT> tiles;
  tiles.reserve(to_sizet(std::distance(range.begin(), range.end())));
  std::transform(range.begin(), range.end(), std::back_inserter(tiles),
                 [&](auto idx) { return f(idx); });
  return tiles;
}
}

/// A @c Matrix object represents a collection of tiles which contain all the elements of a matrix.
///
/// The tiles are distributed according to a distribution (see @c Matrix::distribution()),
/// therefore some tiles are stored locally on this rank,
/// while the others are available on other ranks.
template <class T, Device D>
class Matrix : public Matrix<const T, D> {
public:
  static constexpr Device device = D;

  using ElementType = T;
  using TileType = Tile<ElementType, D>;
  using ConstTileType = Tile<const ElementType, D>;
  using TileDataType = internal::TileData<const ElementType, D>;
  using ReadWriteSenderType = ReadWriteTileSender<T, D>;
  friend Matrix<const ElementType, D>;

  /// Create a non distributed matrix of size @p size and block size @p block_size.
  ///
  /// @pre size.isValid(),
  /// @pre !blockSize.isEmpty().
  Matrix(const LocalElementSize& size, const TileElementSize& block_size);

  /// Create a distributed matrix of size @p size and block size @p block_size on the given 2D
  /// communicator grid @p comm.
  ///
  /// @pre size.isValid(),
  /// @pre !blockSize.isEmpty().
  Matrix(const GlobalElementSize& size, const TileElementSize& block_size,
         const comm::CommunicatorGrid& comm);

  /// Create a matrix distributed according to the distribution @p distribution.
  Matrix(Distribution distribution);

  /// Create a matrix distributed according to the distribution @p distribution,
  /// specifying the layout.
  ///
  /// @param[in] layout is the layout which describes how the elements
  ///            of the local part of the matrix will be stored in memory,
  /// @pre distribution.localSize() == layout.size(),
  /// @pre distribution.blockSize() == layout.blockSize().
  Matrix(Distribution distribution, const LayoutInfo& layout) noexcept;

  /// Create a non distributed matrix,
  /// which references elements that are already allocated in the memory.
  ///
  /// @param[in] layout is the layout which describes how the elements
  ///            of the local part of the matrix are stored in memory,
  /// @param[in] ptr is the pointer to the first element of the local part of the matrix,
  /// @pre @p ptr refers to an allocated memory region of at least @c layout.minMemSize() elements.
  Matrix(const LayoutInfo& layout, ElementType* ptr);

  /// Create a matrix distributed according to the distribution @p distribution,
  /// which references elements that are already allocated in the memory.
  ///
  /// @param[in] layout is the layout which describes how the elements
  ///            of the local part of the matrix are stored in memory,
  /// @param[in] ptr is the pointer to the first element of the local part of the matrix,
  /// @pre @p distribution.localSize() == @p layout.size(),
  /// @pre @p distribution.blockSize() == @p layout.blockSize(),
  /// @pre @p ptr refers to an allocated memory region of at least @c layout.minMemSize() elements.
  Matrix(Distribution distribution, const LayoutInfo& layout, ElementType* ptr) noexcept;

  Matrix(const Matrix& rhs) = delete;
  Matrix(Matrix&& rhs) = default;

  Matrix& operator=(const Matrix& rhs) = delete;
  Matrix& operator=(Matrix&& rhs) = default;

  /// Returns a sender of the Tile with local index @p index.
  ///
  /// @pre index.isIn(distribution().localNrTiles()).
  ReadWriteSenderType readwrite(const LocalTileIndex& index) noexcept {
    return tile_managers_[tileLinearIndex(index)].readwrite();
  }

  /// Returns a sender of the Tile with global index @p index.
  ///
  /// @pre the global tile is stored in the current process,
  /// @pre index.isIn(globalNrTiles()).
  ReadWriteSenderType readwrite(const GlobalTileIndex& index) noexcept {
    return readwrite(this->distribution().localTileIndex(index));
  }

protected:
  using Matrix<const T, D>::tileLinearIndex;

private:
  using Matrix<const T, D>::setUpTiles;
  using Matrix<const T, D>::tile_managers_;
};

template <class T, Device D>
class Matrix<const T, D> : public internal::MatrixBase {
public:
  static constexpr Device device = D;

  using ElementType = T;
  using TileType = Tile<ElementType, D>;
  using ConstTileType = Tile<const ElementType, D>;
  using TileDataType = internal::TileData<ElementType, D>;
  using ReadOnlySenderType = ReadOnlyTileSender<T, D>;
  friend Matrix<ElementType, D>;

  Matrix(const LayoutInfo& layout, ElementType* ptr);

  Matrix(const LayoutInfo& layout, const ElementType* ptr)
      : Matrix(layout, const_cast<ElementType*>(ptr)) {}

  Matrix(Distribution distribution, const LayoutInfo& layout, ElementType* ptr) noexcept;

  Matrix(Distribution distribution, const LayoutInfo& layout, const ElementType* ptr)
      : Matrix(std::move(distribution), layout, const_cast<ElementType*>(ptr)) {}

  Matrix(const Matrix& rhs) = delete;
  Matrix(Matrix&& rhs) = default;

  Matrix& operator=(const Matrix& rhs) = delete;
  Matrix& operator=(Matrix&& rhs) = default;

  /// Returns a read-only sender of the Tile with local index @p index.
  ///
  /// @pre index.isIn(distribution().localNrTiles()).
  ReadOnlySenderType read(const LocalTileIndex& index) noexcept {
    return tile_managers_[tileLinearIndex(index)].read();
  }

  /// Returns a read-only sender of the Tile with global index @p index.
  ///
  /// @pre the global tile is stored in the current process,
  /// @pre index.isIn(globalNrTiles()).
  ReadOnlySenderType read(const GlobalTileIndex& index) {
    return read(distribution().localTileIndex(index));
  }

  /// Synchronization barrier for all local tiles in the matrix
  ///
  /// This blocking call does not return until all operations, i.e. both RO and RW,
  /// involving any of the locally available tiles are completed.
  void waitLocalTiles() noexcept;

protected:
  Matrix(Distribution distribution) : internal::MatrixBase{std::move(distribution)} {}

  void setUpTiles(const memory::MemoryView<ElementType, D>& mem, const LayoutInfo& layout) noexcept;

  std::vector<internal::TilePipeline<T, D>> tile_managers_;
};

// Note: the templates of the following helper functions are inverted w.r.t. the Matrix templates
// to allow the user to only specify the device and let the compiler deduce the type T.

// Local versions

/// Create a non distributed matrix of size @p size and block size @p block_size
/// which references elements
/// that are already allocated in the memory with a column major layout.
///
/// @param[in] ld the leading dimension of the matrix,
/// @param[in] ptr is the pointer to the first element of the local part of the matrix,
/// @pre ld >= max(1, size.row()),
/// @pre @p ptr refers to an allocated memory region which can contain the elements of the local matrix
/// stored in the given layout.
template <Device D, class T>
Matrix<T, D> createMatrixFromColMajor(const LocalElementSize& size, const TileElementSize& block_size,
                                      SizeType ld, T* ptr) {
  return Matrix<T, D>(colMajorLayout(size, block_size, ld), ptr);
}

/// Create a non distributed matrix of size @p size and block size @p block_size
/// which references elements
/// that are already allocated in the memory with a tile layout.
///
/// @param[in] ptr is the pointer to the first element of the local part of the matrix,
/// @pre @p ptr refers to an allocated memory region which can contain the elements of the local matrix
/// stored in the given layout.
template <Device D, class T>
Matrix<T, D> createMatrixFromTile(const LocalElementSize& size, const TileElementSize& block_size,
                                  T* ptr) {
  return Matrix<T, D>(tileLayout(size, block_size), ptr);
}

/// Create a non distributed matrix of size @p size and block size @p block_size
/// which references elements
/// that are already allocated in the memory with a tile layout.
///
/// @param[in] ld_tile the leading dimension of the tiles,
/// @param[in] tiles_per_col the number of tiles stored for each column of tiles,
/// @param[in] ptr is the pointer to the first element of the local part of the matrix,
/// @pre @p ld_tile >= max(1, min(block_size.row(), size.row())),
/// @pre @p tiles_per_col >= ceilDiv(size.row(), block_size.col()),
/// @pre @p ptr refers to an allocated memory region which can contain the elements of the local matrix
/// stored in the given layout.
template <Device D, class T>
Matrix<T, D> createMatrixFromTile(const LocalElementSize& size, const TileElementSize& block_size,
                                  SizeType ld_tile, SizeType tiles_per_col, T* ptr) {
  return Matrix<T, D>(tileLayout(size, block_size, ld_tile, tiles_per_col), ptr);
}

// Distributed versions

/// Create a distributed matrix of size @p size and block size @p block_size
/// on the given 2D communicator grid @p comm which references elements
/// that are already allocated in the memory with a column major layout.
///
/// @param[in] ld the leading dimension of the matrix,
/// @param[in] source_rank_index is the rank of the process which contains the top left tile of the matrix,
/// @param[in] ptr is the pointer to the first element of the local part of the matrix,
/// @pre @p ld >= max(1, size.row()),
/// @pre @p source_rank_index.isIn(grid_size),
/// @pre @p ptr refers to an allocated memory region which can contain the elements of the local matrix
/// stored in the given layout.
template <Device D, class T>
Matrix<T, D> createMatrixFromColMajor(const GlobalElementSize& size, const TileElementSize& block_size,
                                      SizeType ld, const comm::CommunicatorGrid& comm,
                                      const comm::Index2D& source_rank_index, T* ptr) {
  Distribution distribution(size, block_size, comm.size(), comm.rank(), source_rank_index);
  auto layout = colMajorLayout(distribution.localSize(), block_size, ld);

  return Matrix<T, D>(std::move(distribution), layout, ptr);
}

/// Create a distributed matrix of size @p size and block size @p block_size
/// on the given 2D communicator grid @p comm which references elements
/// that are already allocated in the memory with a column major layout.
///
/// This method assumes @p source_rank_index to be {0,0}.
/// @param[in] ld the leading dimension of the matrix,
/// @param[in] ptr is the pointer to the first element of the local part of the matrix,
/// @pre @p ld >= max(1, size.row()),
/// @pre @p ptr refers to an allocated memory region which can contain the elements of the local matrix
/// stored in the given layout.
template <Device D, class T>
Matrix<T, D> createMatrixFromColMajor(const GlobalElementSize& size, const TileElementSize& block_size,
                                      SizeType ld, const comm::CommunicatorGrid& comm, T* ptr) {
  return createMatrixFromColMajor<D>(size, block_size, ld, comm, {0, 0}, ptr);
}

/// Create a distributed matrix of size @p size and block size @p block_size
/// on the given 2D communicator grid @p comm which references elements
/// that are already allocated in the memory with a tile layout.
///
/// @param[in] source_rank_index is the rank of the process which contains the top left tile of the matrix,
/// @param[in] ptr is the pointer to the first element of the local part of the matrix,
/// @pre @p source_rank_index.isIn(grid_size),
/// @pre @p ptr refers to an allocated memory region which can contain the elements of the local matrix
/// stored in the given layout.
template <Device D, class T>
Matrix<T, D> createMatrixFromTile(const GlobalElementSize& size, const TileElementSize& block_size,
                                  const comm::CommunicatorGrid& comm,
                                  const comm::Index2D& source_rank_index, T* ptr) {
  Distribution distribution(size, block_size, comm.size(), comm.rank(), source_rank_index);
  auto layout = tileLayout(distribution.localSize(), block_size);

  return Matrix<T, D>(std::move(distribution), layout, ptr);
}

/// Create a distributed matrix of size @p size and block size @p block_size
/// on the given 2D communicator grid @p comm which references elements
/// that are already allocated in the memory with a tile layout.
///
/// This method assumes @p source_rank_index to be {0,0}.
/// @param[in] ptr is the pointer to the first element of the local part of the matrix,
/// @pre @p ptr refers to an allocated memory region which can contain the elements of the local matrix
/// stored in the given layout.
template <Device D, class T>
Matrix<T, D> createMatrixFromTile(const GlobalElementSize& size, const TileElementSize& block_size,
                                  const comm::CommunicatorGrid& comm, T* ptr) {
  return createMatrixFromTile<D>(size, block_size, comm, {0, 0}, ptr);
}

/// Create a distributed matrix of size @p size and block size @p block_size
/// on the given 2D communicator grid @p comm which references elements
/// that are already allocated in the memory with a tile layout.
///
/// @param[in] ld_tile the leading dimension of the tiles,
/// @param[in] tiles_per_col the number of tiles stored for each column of tiles,
/// @param[in] source_rank_index is the rank of the process which contains the top left tile of the matrix,
/// @param[in] ptr is the pointer to the first element of the local part of the matrix,
/// @pre @p ld_tile >= max(1, min(block_size.row(), size.row())),
/// @pre @p tiles_per_col >= ceilDiv(size.row(), block_size.row()),
/// @pre @p source_rank_index.isIn(grid_size),
/// @pre @p ptr refers to an allocated memory region which can contain the elements of the local matrix
/// stored in the given layout.
template <Device D, class T>
Matrix<T, D> createMatrixFromTile(const GlobalElementSize& size, const TileElementSize& block_size,
                                  SizeType ld_tile, SizeType tiles_per_col,
                                  const comm::CommunicatorGrid& comm,
                                  const comm::Index2D& source_rank_index, T* ptr) {
  Distribution distribution(size, block_size, comm.size(), comm.rank(), source_rank_index);
  auto layout = tileLayout(distribution.localSize(), block_size, ld_tile, tiles_per_col);

  return Matrix<T, D>(std::move(distribution), layout, ptr);
}

/// Create a distributed matrix of size @p size and block size @p block_size
/// on the given 2D communicator grid @p comm which references elements
/// that are already allocated in the memory with a tile layout.
///
/// This method assumes @p source_rank_index to be {0,0}.
/// @param[in] ld_tile the leading dimension of the tiles,
/// @param[in] tiles_per_col the number of tiles stored for each column of tiles,
/// @param[in] ptr is the pointer to the first element of the local part of the matrix,
/// @pre @p ld_tile >= max(1, min(block_size.row(), size.row()),
/// @pre @p tiles_per_col >= ceilDiv(size.row(), block_size.col()),
/// @pre @p ptr refers to an allocated memory region which can contain the elements of the local matrix
/// stored in the given layout.
template <Device D, class T>
Matrix<T, D> createMatrixFromTile(const GlobalElementSize& size, const TileElementSize& block_size,
                                  SizeType ld_tile, SizeType tiles_per_col,
                                  const comm::CommunicatorGrid& comm, T* ptr) {
  return createMatrixFromTile<D>(size, block_size, ld_tile, tiles_per_col, comm, {0, 0}, ptr);
}

/// Returns a container grouping all the tiles retrieved using Matrix::read
///
/// @pre @p range must be a valid range for @p matrix
template <class MatrixLike>
auto selectRead(MatrixLike& matrix, common::IterableRange2D<SizeType, LocalTile_TAG> range) {
  return internal::selectGeneric([&](auto index) { return matrix.read(index); }, range);
}

/// Returns a container grouping all the tiles retrieved using Matrix::operator()
///
/// @pre @p range must be a valid range for @p matrix
template <class MatrixLike>
auto select(MatrixLike& matrix, common::IterableRange2D<SizeType, LocalTile_TAG> range) {
  return internal::selectGeneric([&](auto index) { return matrix.readwrite(index); }, range);
}

// ETI

#define DLAF_MATRIX_ETI(KWORD, DATATYPE, DEVICE) \
  KWORD template class Matrix<DATATYPE, DEVICE>; \
  KWORD template class Matrix<const DATATYPE, DEVICE>;

DLAF_MATRIX_ETI(extern, float, Device::CPU)
DLAF_MATRIX_ETI(extern, double, Device::CPU)
DLAF_MATRIX_ETI(extern, std::complex<float>, Device::CPU)
DLAF_MATRIX_ETI(extern, std::complex<double>, Device::CPU)

#if defined(DLAF_WITH_GPU)
DLAF_MATRIX_ETI(extern, float, Device::GPU)
DLAF_MATRIX_ETI(extern, double, Device::GPU)
DLAF_MATRIX_ETI(extern, std::complex<float>, Device::GPU)
DLAF_MATRIX_ETI(extern, std::complex<double>, Device::GPU)
#endif
}
#ifndef DLAF_DOXYGEN
// Note: Doxygen doesn't deal correctly with template specialized inheritance,
// and this line makes it run infinitely

/// Make dlaf::matrix::Matrix available as dlaf::Matrix.
using matrix::Matrix;
#endif
}

#include <dlaf/matrix/matrix.tpp>
#include <dlaf/matrix/matrix_const.tpp>
