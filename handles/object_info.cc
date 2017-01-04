// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/mtl/handles/object_info.h"

#include <magenta/syscalls.h>
#include <magenta/syscalls/object.h>
#include <magenta/threads.h>

namespace mtl {

mx_koid_t GetKoid(mx_handle_t handle) {
  mx_info_handle_basic_t info;
  mx_status_t status = mx_object_get_info(handle, MX_INFO_HANDLE_BASIC, &info,
                                          sizeof(info), nullptr, nullptr);
  return status == NO_ERROR ? info.koid : MX_KOID_INVALID;
}

mx_obj_type_t GetObjectType(mx_handle_t handle) {
  mx_info_handle_basic_t info;
  mx_status_t status = mx_object_get_info(handle, MX_INFO_HANDLE_BASIC, &info,
                                          sizeof(info), nullptr, nullptr);
  return status == NO_ERROR ? static_cast<mx_obj_type_t>(info.type)
                            : MX_OBJ_TYPE_NONE;
}

std::string GetObjectName(mx_handle_t handle) {
  char name[MX_MAX_NAME_LEN];
  mx_status_t status =
      mx_object_get_property(handle, MX_PROP_NAME, name, sizeof(name));
  return status == NO_ERROR ? std::string(name) : std::string();
}

mx_status_t SetObjectName(mx_handle_t handle, const std::string& name) {
  return mx_object_set_property(handle, MX_PROP_NAME, name.c_str(),
                                name.size());
}

mx_koid_t GetCurrentProcessKoid() {
  return GetKoid(mx_process_self());
}

std::string GetCurrentProcessName() {
  return GetObjectName(mx_process_self());
}

mx_koid_t GetCurrentThreadKoid() {
  return GetKoid(thrd_get_mx_handle(thrd_current()));
}

std::string GetCurrentThreadName() {
  return GetObjectName(thrd_get_mx_handle(thrd_current()));
}

mx_status_t SetCurrentThreadName(const std::string& name) {
  return SetObjectName(thrd_get_mx_handle(thrd_current()), name);
}

}  // namespace mtl
