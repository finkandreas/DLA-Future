//
// Distributed Linear Algebra with Future (DLAF)
//
// Copyright (c) 2018-2022, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause
//
#pragma once

#include <pika/unwrap.hpp>
#include <type_traits>

#include "dlaf/common/pipeline.h"
#include "dlaf/communication/communicator.h"
#include "dlaf/sender/transform.h"
#include "dlaf/sender/when_all_lift.h"

namespace dlaf::comm::internal {

template <class T>
struct IsPromiseGuard : std::false_type {};

template <class T>
struct IsPromiseGuard<dlaf::common::PromiseGuard<T>> : std::true_type {};

template <class T>
inline constexpr bool IsPromiseGuard_v = IsPromiseGuard<T>::value;

template <class T>
struct IsPromiseGuardCommunicator : std::is_same<T, dlaf::common::PromiseGuard<Communicator>> {};

template <class T>
inline constexpr bool IsPromiseGuardCommunicator_v = IsPromiseGuardCommunicator<T>::value;

/// This helper unwraps a PromiseGuard<T> and returns the reference to the object of type T
/// inside. Lifetime of the PromiseGuard has to be ensured by the caller.
template <typename T>
decltype(auto) unwrapPromiseGuard(T& t) {
  if constexpr (IsPromiseGuard_v<std::decay_t<T>>) {
    return t.ref();
  }
  else {
    return static_cast<T&>(t);
  }
}

/// This helper "consumes" a PromiseGuard<Communicator> ensuring that after this call the one
/// passed as argument gets destroyed.
template <typename T>
void consumePromiseGuardCommunicator(T& t) {
  if constexpr (IsPromiseGuardCommunicator_v<std::decay_t<T>>) {
    [[maybe_unused]] auto t_local = std::move(t);
  }
}

/// Helper type for wrapping MPI calls.
///
/// Wrapper type around calls to MPI functions. Provides a call operator that
/// creates an MPI request and passes it as the last argument to the provided
/// callable. The wrapper then waits for the the request to complete with
/// yield_while.
///
/// This could in theory be a lambda inside transformMPI.  However, clang at
/// least until version 12 fails with an internal compiler error with a trailing
/// decltype for SFINAE. GCC has no problems with a lambda.
template <typename F>
struct MPICallHelper {
  std::decay_t<F> f;
  template <typename... Ts>
  auto operator()(Ts&&... ts) -> decltype(pika::unwrapping(std::move(f))(
      unwrapPromiseGuard(dlaf::internal::getReferenceWrapper(ts))..., std::declval<MPI_Request*>())) {
    MPI_Request req;
    auto is_request_completed = [&req] {
      int flag;
      MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
      return flag == 0;
    };

    // Note:
    // Callables passed to transformMPI have their arguments passed by reference, but doing so
    // with PromiseGuard would keep the guard alive until the completion of the MPI operation,
    // whereas we are only looking to guard the submission of the MPI operation.
    // Moreover, the callable requires a Communicator to make its job, and it should be agnostic of
    // it being wrapped in a PromiseGuard or not.
    // We therefore use unwrapPromiseGuard to pass the Communicator& into a transformMPI
    // callables, then after returning from the callable, the PromiseGuard<Communicator> gets explicitly
    // released using the helper consumePromiseGuardCommunicator.
    using result_type = decltype(pika::unwrapping(
        std::move(f))(unwrapPromiseGuard(dlaf::internal::getReferenceWrapper(ts))..., &req));
    if constexpr (std::is_void_v<result_type>) {
      pika::unwrapping(std::move(f))(unwrapPromiseGuard(dlaf::internal::getReferenceWrapper(ts))...,
                                     &req);
      (internal::consumePromiseGuardCommunicator(dlaf::internal::getReferenceWrapper(ts)), ...);
      pika::util::yield_while(is_request_completed);
    }
    else {
      auto r = pika::unwrapping(
          std::move(f))(unwrapPromiseGuard(dlaf::internal::getReferenceWrapper(ts))..., &req);
      (internal::consumePromiseGuardCommunicator(dlaf::internal::getReferenceWrapper(ts)), ...);
      pika::util::yield_while(is_request_completed);
      return r;
    }
  }
};

template <typename F>
MPICallHelper(F &&) -> MPICallHelper<std::decay_t<F>>;

/// Lazy transformMPI. This does not submit the work and returns a sender.
template <typename F, typename Sender,
          typename = std::enable_if_t<pika::execution::experimental::is_sender_v<Sender>>>
[[nodiscard]] decltype(auto) transformMPI(F&& f, Sender&& sender) {
  namespace ex = pika::execution::experimental;

  return ex::transfer(std::forward<Sender>(sender), dlaf::internal::getMPIScheduler()) |
         ex::then(MPICallHelper{std::forward<F>(f)});
}

/// Fire-and-forget transformMPI. This submits the work and returns void.
template <typename F, typename Sender,
          typename = std::enable_if_t<pika::execution::experimental::is_sender_v<Sender>>>
void transformMPIDetach(F&& f, Sender&& sender) {
  pika::execution::experimental::start_detached(
      transformMPI(std::forward<F>(f), std::forward<Sender>(sender)));
}

/// Lazy transformMPI. This does not submit the work and returns a sender. First
/// lifts non-senders into senders using just, and then calls transform with a
/// when_all sender of the lifted senders.
template <typename F, typename... Ts>
[[nodiscard]] decltype(auto) transformMPILift(F&& f, Ts&&... ts) {
  return transformMPI(std::forward<F>(f), dlaf::internal::whenAllLift(std::forward<Ts>(ts)...));
}

/// Fire-and-forget transformMPI. This submits the work and returns void. First
/// lifts non-senders into senders using just, and then calls transform with a
/// when_all sender of the lifted senders.
template <typename F, typename... Ts>
void transformMPILiftDetach(F&& f, Ts&&... ts) {
  pika::execution::experimental::start_detached(
      transformLift(std::forward<F>(f), std::forward<Ts>(ts)...));
}

template <typename F>
struct PartialTransformMPIBase {
  std::decay_t<F> f_;
};

/// A partially applied transformMPI, with the callable object given, but the
/// predecessor sender missing. The predecessor sender is applied when calling
/// the operator| overload.
template <typename F>
class PartialTransformMPI : private PartialTransformMPIBase<F> {
public:
  template <typename F_>
  PartialTransformMPI(F_&& f) : PartialTransformMPIBase<F>{std::forward<F_>(f)} {}
  PartialTransformMPI(PartialTransformMPI&&) = default;
  PartialTransformMPI(PartialTransformMPI const&) = default;
  PartialTransformMPI& operator=(PartialTransformMPI&&) = default;
  PartialTransformMPI& operator=(PartialTransformMPI const&) = default;

  template <typename Sender>
  friend auto operator|(Sender&& sender, const PartialTransformMPI pa) {
    return transformMPI(std::move(pa.f_), std::forward<Sender>(sender));
  }
};

template <typename F>
PartialTransformMPI(F&& f) -> PartialTransformMPI<std::decay_t<F>>;

/// A partially applied transformMPIDetach, with the callable object given, but
/// the predecessor sender missing. The predecessor sender is applied when
/// calling the operator| overload.
template <typename F>
class PartialTransformMPIDetach : private PartialTransformMPIBase<F> {
public:
  template <typename F_>
  PartialTransformMPIDetach(F_&& f) : PartialTransformMPIBase<F>{std::forward<F_>(f)} {}
  PartialTransformMPIDetach(PartialTransformMPIDetach&&) = default;
  PartialTransformMPIDetach(PartialTransformMPIDetach const&) = default;
  PartialTransformMPIDetach& operator=(PartialTransformMPIDetach&&) = default;
  PartialTransformMPIDetach& operator=(PartialTransformMPIDetach const&) = default;

  template <typename Sender>
  friend auto operator|(Sender&& sender, const PartialTransformMPIDetach pa) {
    return pika::execution::experimental::start_detached(
        transformMPI(std::move(pa.f_), std::forward<Sender>(sender)));
  }
};

template <typename F>
PartialTransformMPIDetach(F&& f) -> PartialTransformMPIDetach<std::decay_t<F>>;

/// \overload transformMPI
///
/// This overload partially applies the MPI transform for later use with
/// operator| with a sender on the left-hand side.
template <typename F>
[[nodiscard]] decltype(auto) transformMPI(F&& f) {
  return PartialTransformMPI{std::forward<F>(f)};
}

/// \overload transformMPIDetach
///
/// This overload partially applies transformMPIDetach for later use with
/// operator| with a sender on the left-hand side.
template <typename F>
[[nodiscard]] decltype(auto) transformMPIDetach(F&& f) {
  return PartialTransformMPIDetach{std::forward<F>(f)};
}
}
