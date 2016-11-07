// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/mtl/fidl_data_pipe/vector.h"

#include <utility>

#include "lib/ftl/logging.h"
#include "lib/mtl/fidl_data_pipe/blocking_copy.h"

namespace mtl {

bool FidlBlockingCopyToVector(mx::datapipe_consumer source,
                              std::vector<char>* result) {
  FTL_CHECK(result);
  result->clear();
  return FidlBlockingCopyFrom(
      std::move(source), [result](const void* buffer, uint32_t num_bytes) {
        const char* chars = static_cast<const char*>(buffer);
        result->insert(result->end(), chars, chars + num_bytes);
        return num_bytes;
      });
}

}  // namespace mtl
