// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/mtl/fidl_data_pipe/data_pipe_drainer.h"

#include <utility>

#include "lib/ftl/logging.h"
#include "mx/datapipe.h"

namespace mtl {

FidlDataPipeDrainer::FidlDataPipeDrainer(Client* client,
                                         const FidlAsyncWaiter* waiter)
    : client_(client), waiter_(waiter), wait_id_(0) {
  FTL_DCHECK(client_);
}

FidlDataPipeDrainer::~FidlDataPipeDrainer() {
  if (wait_id_)
    waiter_->CancelWait(wait_id_);
}

void FidlDataPipeDrainer::Start(mx::datapipe_consumer source) {
  source_ = std::move(source);
  ReadData();
}

void FidlDataPipeDrainer::ReadData() {
  void* buffer = 0u;
  mx_size_t num_bytes = 0;
  mx_status_t rv =
      source_.begin_read(0, reinterpret_cast<uintptr_t*>(&buffer), &num_bytes);
  if (rv == NO_ERROR) {
    client_->OnDataAvailable(buffer, num_bytes);
    source_.end_read(num_bytes);
    WaitForData();
  } else if (rv == ERR_SHOULD_WAIT) {
    WaitForData();
  } else if (rv == ERR_REMOTE_CLOSED) {
    client_->OnDataComplete();
  } else {
    FTL_DCHECK(false) << "Unhandled mx_status_t: " << rv;
  }
}

void FidlDataPipeDrainer::WaitForData() {
  FTL_DCHECK(!wait_id_);
  wait_id_ = waiter_->AsyncWait(source_.get(),
                                MX_SIGNAL_READABLE | MX_SIGNAL_PEER_CLOSED,
                                MX_TIME_INFINITE, &WaitComplete, this);
}

void FidlDataPipeDrainer::WaitComplete(mx_status_t result,
                                       mx_signals_t pending,
                                       void* context) {
  FidlDataPipeDrainer* drainer = static_cast<FidlDataPipeDrainer*>(context);
  drainer->wait_id_ = 0;
  drainer->ReadData();
}

}  // namespace mtl
