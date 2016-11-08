// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_MTL_DATA_PIPE_VECTOR_H_
#define LIB_MTL_DATA_PIPE_VECTOR_H_

#include <vector>

#include "mx/datapipe.h"

namespace mtl {

// Copies the data from |source| into |contents| and returns true on success and
// false on error. In case of I/O error, |contents| holds the data that could
// be read from source before the error occurred.
bool BlockingCopyToVector(mx::datapipe_consumer source,
                          std::vector<char>* contents);

}  // namespace mtl

#endif  // LIB_MTL_DATA_PIPE_VECTOR_H_
