// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/mtl/tasks/fd_waiter.h"

#include <magenta/errors.h>

namespace mtl {

FDWaiter::FDWaiter() : io_(nullptr), key_(0) { }

FDWaiter::~FDWaiter() {
  if (io_) {
    Cancel();
  }

  FTL_DCHECK(!io_);
  FTL_DCHECK(!key_);
}

bool FDWaiter::Wait(Callback callback,
                    int fd,
                    uint32_t events,
                    ftl::TimeDelta timeout) {
  FTL_DCHECK(!io_);
  FTL_DCHECK(!key_);

  io_ = __mxio_fd_to_io(fd);

  mx_handle_t handle = MX_HANDLE_INVALID;
  mx_signals_t signals = MX_SIGNAL_NONE;
  __mxio_wait_begin(io_, events, &handle, &signals);

  if (handle == MX_HANDLE_INVALID) {
    Cancel();
    return false;
  }

  key_ = MessageLoop::GetCurrent()->AddHandler(this, handle, signals, timeout);

  // Last to prevent re-entrancy from the move constructor of the callback.
  callback_ = std::move(callback);
  return true;
}

void FDWaiter::Cancel() {
  FTL_DCHECK(io_);
  FTL_DCHECK(key_);

  MessageLoop::GetCurrent()->RemoveHandler(key_);

  __mxio_release(io_);
  io_ = nullptr;
  key_ = 0;

  // Last to prevent re-entrancy from the destructor of the callback.
  callback_ = Callback();
}

void FDWaiter::OnHandleReady(mx_handle_t handle,
                             mx_signals_t pending,
                             uint64_t count) {
  FTL_DCHECK(io_);
  FTL_DCHECK(key_);

  uint32_t events = 0;
  __mxio_wait_end(io_, pending, &events);

  Callback callback = std::move(callback_);
  Cancel();

  // Last to prevent re-entrancy from the callback.
  callback(MX_OK, events);
}

void FDWaiter::OnHandleError(mx_handle_t handle, mx_status_t error) {
  Callback callback = std::move(callback_);
  Cancel();

  // Last to prevent re-entrancy from the callback.
  callback(error, 0);
}

}  // namespace mtl
