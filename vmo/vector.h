// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_MTL_VMO_VECTOR_H_
#define LIB_MTL_VMO_VECTOR_H_

#include <mx/vmo.h>

#include <vector>

#include "lib/ftl/ftl_export.h"

namespace mtl {

// Make a new shared buffer with the contents of a char vector.
FTL_EXPORT bool VmoFromVector(const std::vector<char>& vector,
                              mx::vmo* handle_ptr);

// Copy the contents of a shared buffer into a char vector.
FTL_EXPORT bool VectorFromVmo(const mx::vmo& shared_buffer,
                              std::vector<char>* vector_ptr);

// Make a new shared buffer with the contents of a uint8_t vector.
FTL_EXPORT bool VmoFromVector(const std::vector<uint8_t>& vector,
                              mx::vmo* handle_ptr);

// Copy the contents of a shared buffer into a uint8_t vector.
FTL_EXPORT bool VectorFromVmo(const mx::vmo& shared_buffer,
                              std::vector<uint8_t>* vector_ptr);

}  // namespace mtl

#endif  // LIB_MTL_VMO_VECTOR_H_
