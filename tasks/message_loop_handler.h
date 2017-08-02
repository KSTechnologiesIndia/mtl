// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_MTL_TASKS_MESSAGE_LOOP_HANDLER_H_
#define LIB_MTL_TASKS_MESSAGE_LOOP_HANDLER_H_

#include <magenta/types.h>

#include "lib/ftl/ftl_export.h"

namespace mtl {

class FTL_EXPORT MessageLoopHandler {
 public:
  // Called when the handle receives signals that the handler was waiting for.
  //
  // The handler remains in the message loop; it will continue to receive
  // signals until it is removed.
  //
  // The |pending| signals represents a snapshot of the signal state when
  // the wait completed; it will always include at least one of the signals
  // that the handler was waiting for but may also include other signals
  // that happen to also be asserted at the same time.
  //
  // The number of pending operations is passed in |count|. This is the count
  // returned by |mx_port_wait|, see:
  // https://fuchsia.googlesource.com/magenta/+/master/docs/syscalls/port_wait.md
  // For channels this is the number of messages that are ready to be read.
  //
  // The default implementation does nothing.
  virtual void OnHandleReady(mx_handle_t handle,
                             mx_signals_t pending,
                             uint64_t count);

  // Called when an error occurs while waiting on the handle.
  //
  // The handler is automatically removed by the message loop before invoking
  // this callback; it will not receive any further signals unless it is
  // re-added later.
  //
  // Possible errors are:
  //   - |MX_ERR_TIMED_OUT|: the handler deadline elapsed
  //   - |MX_ERR_CANCELED|:  the message loop itself has been destroyed
  //                         rendering it impossible to continue waiting on any
  //                         handles
  //
  // The default implementation does nothing.
  virtual void OnHandleError(mx_handle_t handle, mx_status_t error);

 protected:
  virtual ~MessageLoopHandler();
};

}  // namespace mtl

#endif  // LIB_MTL_TASKS_MESSAGE_LOOP_HANDLER_H_
