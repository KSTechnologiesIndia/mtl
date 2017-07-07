// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mx/socket.h>

#include "gtest/gtest.h"
#include "lib/mtl/socket/socket_drainer.h"
#include "lib/mtl/socket/strings.h"
#include "lib/mtl/tasks/message_loop.h"

namespace mtl {
namespace {

class Client : public SocketDrainer::Client {
 public:
  Client(const std::function<void()>& available_callback,
         const std::function<void()>& completion_callback)
      : available_callback_(available_callback),
        completion_callback_(completion_callback) {}
  ~Client() override {}

  std::string GetValue() { return value_; }

 private:
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    value_.append(static_cast<const char*>(data), num_bytes);
    available_callback_();
  }
  void OnDataComplete() override { completion_callback_(); }

  std::string value_;
  std::function<void()> available_callback_;
  std::function<void()> completion_callback_;
};

TEST(SocketDrainer, ReadData) {
  MessageLoop message_loop;
  Client client([] {}, [&message_loop] { message_loop.QuitNow(); });
  SocketDrainer drainer(&client);
  drainer.Start(mtl::WriteStringToSocket("Hello"));
  message_loop.Run();
  EXPECT_EQ("Hello", client.GetValue());
}

TEST(SocketDrainer, DeleteOnCallback) {
  MessageLoop message_loop;
  std::unique_ptr<SocketDrainer> drainer;
  Client client(
      [&message_loop, &drainer] {
        message_loop.PostQuitTask();
        drainer.reset();
      },
      [] {});
  drainer = std::make_unique<SocketDrainer>(&client);
  drainer->Start(mtl::WriteStringToSocket("H"));
  message_loop.Run();
  EXPECT_EQ("H", client.GetValue());
  EXPECT_EQ(nullptr, drainer.get());
}

}  // namespace
}  // namespace mtl
