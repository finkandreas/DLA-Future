//
// Distributed Linear Algebra with Future (DLAF)
//
// Copyright (c) 2018-2023, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause
//

#include <dlaf/eigensolver/bt_band_to_tridiag/impl.h>

namespace dlaf::eigensolver::internal {

DLAF_EIGENSOLVER_BT_BAND_TO_TRIDIAGONAL_ETI(, Backend::GPU, Device::GPU, float)
DLAF_EIGENSOLVER_BT_BAND_TO_TRIDIAGONAL_ETI(, Backend::GPU, Device::GPU, double)
DLAF_EIGENSOLVER_BT_BAND_TO_TRIDIAGONAL_ETI(, Backend::GPU, Device::GPU, std::complex<float>)
DLAF_EIGENSOLVER_BT_BAND_TO_TRIDIAGONAL_ETI(, Backend::GPU, Device::GPU, std::complex<double>)

}
