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

#include <complex>

#include <mpi.h>

#include <pika/execution.hpp>

#include <dlaf/common/pipeline.h>
#include <dlaf/communication/communicator.h>
#include <dlaf/matrix/matrix.h>
#include <dlaf/matrix/tile.h>
#include <dlaf/sender/traits.h>

namespace dlaf::comm {
/// Schedule an all reduce.
///
/// An input and output tile is required for the reduction. The returned sender
/// signals completion when the reduction is done. The output sender tile must
/// be writable so that the received and reduced data can be written to it. The
/// output tile is sent by the returned sender.
template <class T, Device D>
[[nodiscard]] dlaf::matrix::ReadWriteTileSender<T, D> scheduleAllReduce(
    pika::execution::experimental::unique_any_sender<common::Pipeline<Communicator>::Wrapper> pcomm,
    MPI_Op reduce_op, dlaf::matrix::ReadOnlyTileSender<T, D> tile_in,
    dlaf::matrix::ReadWriteTileSender<T, D> tile_out);

#define DLAF_SCHEDULE_ALL_REDUCE_ETI(kword, Type, Device)                                              \
  kword template dlaf::matrix::ReadWriteTileSender<Type, Device> scheduleAllReduce(                    \
      pika::execution::experimental::unique_any_sender<common::Pipeline<Communicator>::Wrapper> pcomm, \
      MPI_Op reduce_op, dlaf::matrix::ReadOnlyTileSender<Type, Device> tile_in,                        \
      dlaf::matrix::ReadWriteTileSender<Type, Device> tile_out)

DLAF_SCHEDULE_ALL_REDUCE_ETI(extern, int, Device::CPU);

/// Schedule an in-place all reduce.
///
/// The returned sender signals completion when the reduction is done.  The
/// sender tile must be writable so that the received and reduced data can be
/// written to it. The tile is sent by the returned sender.
template <class T, Device D>
[[nodiscard]] dlaf::matrix::ReadWriteTileSender<T, D> scheduleAllReduceInPlace(
    pika::execution::experimental::unique_any_sender<common::Pipeline<Communicator>::Wrapper> pcomm,
    MPI_Op reduce_op, dlaf::matrix::ReadWriteTileSender<T, D> tile);

#define DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(kword, Type, Device)                                     \
  kword template dlaf::matrix::ReadWriteTileSender<Type, Device> scheduleAllReduceInPlace(             \
      pika::execution::experimental::unique_any_sender<common::Pipeline<Communicator>::Wrapper> pcomm, \
      MPI_Op reduce_op, dlaf::matrix::ReadWriteTileSender<Type, Device> tile)

DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(extern, int, Device::CPU);
DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(extern, float, Device::CPU);
DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(extern, double, Device::CPU);
DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(extern, std::complex<float>, Device::CPU);
DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(extern, std::complex<double>, Device::CPU);

#ifdef DLAF_WITH_GPU
DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(extern, float, Device::GPU);
DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(extern, double, Device::GPU);
DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(extern, std::complex<float>, Device::GPU);
DLAF_SCHEDULE_ALL_REDUCE_IN_PLACE_ETI(extern, std::complex<double>, Device::GPU);
#endif
}
