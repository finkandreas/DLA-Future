//
// Distributed Linear Algebra with Future (DLAF)
//
// Copyright (c) 2018-2023, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause
//

#include <complex>
#include <utility>

#include <mpi.h>

#include <pika/execution.hpp>

#include <dlaf/common/assert.h>
#include <dlaf/common/callable_object.h>
#include <dlaf/common/data.h>
#include <dlaf/communication/communicator.h>
#include <dlaf/communication/kernels/broadcast.h>
#include <dlaf/communication/message.h>
#include <dlaf/communication/rdma.h>
#include <dlaf/matrix/tile.h>
#include <dlaf/sender/traits.h>
#include <dlaf/sender/transform_mpi.h>
#include <dlaf/sender/with_temporary_tile.h>

namespace dlaf::comm {
namespace internal {
template <class CommSender, class TileSender>
[[nodiscard]] auto scheduleSendBcast(CommSender&& pcomm, TileSender&& tile) {
  using dlaf::internal::CopyFromDestination;
  using dlaf::internal::CopyToDestination;
  using dlaf::internal::RequireContiguous;
  using dlaf::internal::SenderSingleValueType;
  using dlaf::internal::whenAllLift;
  using dlaf::internal::withTemporaryTile;

  auto send = [pcomm = std::forward<CommSender>(pcomm)](const auto& tile_comm) mutable {
    return whenAllLift(std::move(pcomm), std::cref(tile_comm)) | transformMPI(sendBcast_o);
  };

  constexpr Device in_device_type = SenderSingleValueType<std::decay_t<TileSender>>::device;
  constexpr Device comm_device_type = CommunicationDevice_v<in_device_type>;

  // The input tile must be copied to the temporary tile used for the send, but
  // the temporary tile does not need to be copied back to the input since the
  // data is not changed by the send. A send does not require contiguous memory.
  return withTemporaryTile<comm_device_type, CopyToDestination::Yes, CopyFromDestination::No,
                           RequireContiguous::No>(std::forward<TileSender>(tile), std::move(send));
}
}

template <class T, Device D, class Comm>
[[nodiscard]] pika::execution::experimental::unique_any_sender<> scheduleSendBcast(
    pika::execution::experimental::unique_any_sender<Comm> pcomm,
    dlaf::matrix::ReadOnlyTileSender<T, D> tile) {
  return internal::scheduleSendBcast(std::move(pcomm), std::move(tile));
}

DLAF_SCHEDULE_SEND_BCAST_ETI(, SizeType, Device::CPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_SEND_BCAST_ETI(, float, Device::CPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_SEND_BCAST_ETI(, double, Device::CPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_SEND_BCAST_ETI(, std::complex<float>, Device::CPU,
                             common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_SEND_BCAST_ETI(, std::complex<double>, Device::CPU,
                             common::Pipeline<Communicator>::Wrapper);

#ifdef DLAF_WITH_GPU
DLAF_SCHEDULE_SEND_BCAST_ETI(, SizeType, Device::GPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_SEND_BCAST_ETI(, float, Device::GPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_SEND_BCAST_ETI(, double, Device::GPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_SEND_BCAST_ETI(, std::complex<float>, Device::GPU,
                             common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_SEND_BCAST_ETI(, std::complex<double>, Device::GPU,
                             common::Pipeline<Communicator>::Wrapper);
#endif

template <class T, Device D, class Comm>
[[nodiscard]] dlaf::matrix::ReadWriteTileSender<T, D> scheduleRecvBcast(
    pika::execution::experimental::unique_any_sender<Comm> pcomm, comm::IndexT_MPI root_rank,
    dlaf::matrix::ReadWriteTileSender<T, D> tile) {
  using dlaf::comm::internal::recvBcast_o;
  using dlaf::internal::CopyFromDestination;
  using dlaf::internal::CopyToDestination;
  using dlaf::internal::RequireContiguous;
  using dlaf::internal::SenderSingleValueType;
  using dlaf::internal::whenAllLift;
  using dlaf::internal::withTemporaryTile;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
  auto recv = [root_rank, pcomm = std::move(pcomm)](const auto& tile_comm) mutable {
    return whenAllLift(std::move(pcomm), root_rank, std::cref(tile_comm)) | transformMPI(recvBcast_o);
  };
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

  constexpr Device in_device_type = D;
  constexpr Device comm_device_type = CommunicationDevice_v<in_device_type>;

  // Since this is a receive we don't need to copy the input to the temporary
  // tile (the input tile may be uninitialized). The received data is copied
  // back from the temporary tile to the input. A receive does not require
  // contiguous memory.
  return withTemporaryTile<comm_device_type, CopyToDestination::No, CopyFromDestination::Yes,
                           RequireContiguous::No>(std::move(tile), std::move(recv));
}

DLAF_SCHEDULE_RECV_BCAST_ETI(, SizeType, Device::CPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_RECV_BCAST_ETI(, float, Device::CPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_RECV_BCAST_ETI(, double, Device::CPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_RECV_BCAST_ETI(, std::complex<float>, Device::CPU,
                             common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_RECV_BCAST_ETI(, std::complex<double>, Device::CPU,
                             common::Pipeline<Communicator>::Wrapper);

#ifdef DLAF_WITH_GPU
DLAF_SCHEDULE_RECV_BCAST_ETI(, SizeType, Device::GPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_RECV_BCAST_ETI(, float, Device::GPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_RECV_BCAST_ETI(, double, Device::GPU, common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_RECV_BCAST_ETI(, std::complex<float>, Device::GPU,
                             common::Pipeline<Communicator>::Wrapper);
DLAF_SCHEDULE_RECV_BCAST_ETI(, std::complex<double>, Device::GPU,
                             common::Pipeline<Communicator>::Wrapper);
#endif
}
