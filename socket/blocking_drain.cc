// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/mtl/socket/blocking_drain.h"

#include <vector>

#include "lib/ftl/logging.h"
#include "mx/socket.h"

namespace mtl {

bool BlockingDrainFrom(
    mx::socket source,
    const std::function<size_t(const void*, uint32_t)>& write_bytes) {
  std::vector<char> buffer(64 * 1024);
  for (;;) {
    size_t bytes_read;
    mx_status_t result =
        source.read(0, buffer.data(), buffer.size(), &bytes_read);
    if (result == NO_ERROR) {
      size_t bytes_written = write_bytes(buffer.data(), bytes_read);
      if (bytes_written < bytes_read) {
        FTL_LOG(ERROR) << "write_bytes callback wrote fewer bytes ("
                       << bytes_written << ") than expected (" << bytes_read
                       << ") in BlockingDrainFrom (socket closed? out of disk "
                          "space?)";
        return false;
      }
    } else if (result == ERR_SHOULD_WAIT) {
      result = source.wait_one(MX_SOCKET_READABLE | MX_SOCKET_PEER_CLOSED,
                               MX_TIME_INFINITE, nullptr);
      if (result != NO_ERROR) {
        // If the socket was closed, then treat as EOF.
        return result == ERR_REMOTE_CLOSED;
      }
    } else if (result == ERR_REMOTE_CLOSED) {
      // If the socket was closed, then treat as EOF.
      return true;
    } else {
      FTL_LOG(ERROR) << "Unhandled error " << result << " in BlockingDrainFrom";
      // Some other error occurred.
      return false;
    }
  }
}

}  // namespace mtl
