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

#include <type_traits>
#include <utility>

#include <pika/execution.hpp>

#include <dlaf/types.h>

namespace dlaf {
namespace internal {
/// A policy class for use as a tag for dispatching algorithms to a particular
/// backend.
template <Backend B>
class Policy {
private:
  const pika::execution::thread_priority priority_ = pika::execution::thread_priority::normal;

public:
  Policy() = default;
  explicit Policy(pika::execution::thread_priority priority) : priority_(priority) {}
  Policy(Policy&&) = default;
  Policy(const Policy&) = default;
  Policy& operator=(Policy&&) = default;
  Policy& operator=(const Policy&) = default;

  pika::execution::thread_priority priority() const noexcept {
    return priority_;
  }
};
}
}
