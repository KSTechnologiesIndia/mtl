// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_MTL_SOCKET_FILES_H_
#define LIB_MTL_SOCKET_FILES_H_

#include <mx/socket.h>

#include <functional>

#include "lib/ftl/files/unique_fd.h"
#include "lib/ftl/ftl_export.h"
#include "lib/ftl/tasks/task_runner.h"

namespace mtl {

// Asynchronously copies data from source to the destination file descriptor.
// The given |callback| is run upon completion. File writes and |callback| will
// be scheduled on the given |task_runner|.
FTL_EXPORT void CopyToFileDescriptor(
    mx::socket source,
    ftl::UniqueFD destination,
    ftl::RefPtr<ftl::TaskRunner> task_runner,
    const std::function<void(bool /*success*/, ftl::UniqueFD /*destination*/)>&
        callback);

// Asynchronously copies data from source file to the destination. The given
// |callback| is run upon completion. File reads and |callback| will be
// scheduled to the given |task_runner|.
FTL_EXPORT void CopyFromFileDescriptor(
    ftl::UniqueFD source,
    mx::socket destination,
    ftl::RefPtr<ftl::TaskRunner> task_runner,
    const std::function<void(bool /*success*/, ftl::UniqueFD /*source*/)>&
        callback);

}  // namespace mtl

#endif  // LIB_MTL_SOCKET_FILES_H_
