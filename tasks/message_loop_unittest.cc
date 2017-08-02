// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/mtl/tasks/message_loop.h"

#include <mx/channel.h>
#include <mx/event.h>
#include <mxio/io.h>

#include <poll.h>

#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "lib/ftl/files/unique_fd.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/ftl/macros.h"
#include "lib/mtl/tasks/fd_waiter.h"

namespace mtl {
namespace {

TEST(MessageLoop, Current) {
  EXPECT_TRUE(MessageLoop::GetCurrent() == nullptr);
  {
    MessageLoop message_loop;
    EXPECT_EQ(&message_loop, MessageLoop::GetCurrent());
  }
  EXPECT_TRUE(MessageLoop::GetCurrent() == nullptr);
}

TEST(MessageLoop, RunsTasksOnCurrentThread) {
  MessageLoop loop;
  EXPECT_TRUE(loop.task_runner()->RunsTasksOnCurrentThread());
  bool run_on_other_thread;
  std::thread t([&loop, &run_on_other_thread]() {
    run_on_other_thread = loop.task_runner()->RunsTasksOnCurrentThread();
  });
  t.join();
  EXPECT_FALSE(run_on_other_thread);
}

TEST(MessageLoop, CanRunTasks) {
  bool did_run = false;
  MessageLoop loop;
  loop.task_runner()->PostTask([&did_run, &loop]() {
    EXPECT_FALSE(did_run);
    did_run = true;
    loop.QuitNow();
  });
  loop.Run();
  EXPECT_TRUE(did_run);
}

TEST(MessageLoop, CanPostTasksFromTasks) {
  bool did_run = false;
  MessageLoop loop;
  ftl::Closure nested_task = [&did_run, &loop]() {
    EXPECT_FALSE(did_run);
    did_run = true;
    loop.QuitNow();
  };
  loop.task_runner()->PostTask(
      [&nested_task, &loop]() { loop.task_runner()->PostTask(nested_task); });
  loop.Run();
  EXPECT_TRUE(did_run);
}

TEST(MessageLoop, TriplyNestedTasks) {
  std::vector<std::string> tasks;
  MessageLoop loop;
  loop.task_runner()->PostTask([&tasks, &loop]() {
    tasks.push_back("one");
    loop.task_runner()->PostTask([&tasks, &loop]() {
      tasks.push_back("two");
      loop.task_runner()->PostTask([&tasks, &loop]() {
        tasks.push_back("three");
        loop.QuitNow();
      });
    });
  });
  loop.Run();
  EXPECT_EQ(3u, tasks.size());
  EXPECT_EQ("one", tasks[0]);
  EXPECT_EQ("two", tasks[1]);
  EXPECT_EQ("three", tasks[2]);
}

TEST(MessageLoop, CanRunTasksInOrder) {
  std::vector<std::string> tasks;
  MessageLoop loop;
  loop.task_runner()->PostTask([&tasks]() { tasks.push_back("0"); });
  loop.task_runner()->PostTask([&tasks]() { tasks.push_back("1"); });
  loop.PostQuitTask();
  loop.task_runner()->PostTask([&tasks]() { tasks.push_back("2"); });
  loop.Run();
  EXPECT_EQ(2u, tasks.size());
  EXPECT_EQ("0", tasks[0]);
  EXPECT_EQ("1", tasks[1]);
}

TEST(MessageLoop, CanPreloadTasks) {
  auto incoming_queue = ftl::MakeRefCounted<internal::IncomingTaskQueue>();

  bool did_run = false;
  MessageLoop* loop_ptr = nullptr;
  incoming_queue->PostTask([&did_run, &loop_ptr]() {
    EXPECT_FALSE(did_run);
    did_run = true;
    loop_ptr->QuitNow();
  });

  MessageLoop loop(std::move(incoming_queue));
  loop_ptr = &loop;
  loop.Run();
  EXPECT_TRUE(did_run);
}

TEST(MessageLoop, AfterTaskCallbacks) {
  std::vector<std::string> tasks;
  MessageLoop loop;
  loop.SetAfterTaskCallback([&tasks] { tasks.push_back("callback"); });
  loop.task_runner()->PostTask([&tasks] { tasks.push_back("0"); });
  loop.task_runner()->PostTask([&tasks] { tasks.push_back("1"); });
  loop.PostQuitTask();
  loop.task_runner()->PostTask([&tasks] { tasks.push_back("2"); });
  loop.Run();
  EXPECT_EQ(5u, tasks.size());
  EXPECT_EQ("0", tasks[0]);
  EXPECT_EQ("callback", tasks[1]);
  EXPECT_EQ("1", tasks[2]);
  EXPECT_EQ("callback", tasks[3]);
}

TEST(MessageLoop, RemoveAfterTaskCallbacksDuringCallback) {
  std::vector<std::string> tasks;
  MessageLoop loop;

  loop.SetAfterTaskCallback([&tasks, &loop]() {
    tasks.push_back("callback");
    loop.ClearAfterTaskCallback();
  });
  loop.task_runner()->PostTask([&tasks] { tasks.push_back("0"); });
  loop.task_runner()->PostTask([&tasks] { tasks.push_back("1"); });
  loop.PostQuitTask();
  loop.Run();
  EXPECT_EQ(3u, tasks.size());
  EXPECT_EQ("0", tasks[0]);
  EXPECT_EQ("callback", tasks[1]);
  EXPECT_EQ("1", tasks[2]);
}

struct DestructorObserver {
  DestructorObserver(bool* destructed) : destructed_(destructed) {
    *destructed_ = false;
  }
  ~DestructorObserver() { *destructed_ = true; }

  bool* destructed_;
};

TEST(MessageLoop, TaskDestructionTime) {
  bool destructed = false;
  ftl::RefPtr<ftl::TaskRunner> task_runner;

  {
    MessageLoop loop;
    task_runner = ftl::RefPtr<ftl::TaskRunner>(loop.task_runner());
    loop.PostQuitTask();
    loop.Run();
    auto observer1 = std::make_unique<DestructorObserver>(&destructed);
    task_runner->PostTask(ftl::MakeCopyable([p = std::move(observer1)](){}));
    EXPECT_FALSE(destructed);
  }

  EXPECT_TRUE(destructed);
  auto observer2 = std::make_unique<DestructorObserver>(&destructed);
  EXPECT_FALSE(destructed);
  task_runner->PostTask(ftl::MakeCopyable([p = std::move(observer2)](){}));
  EXPECT_TRUE(destructed);
}

TEST(MessageLoop, CanQuitCurrent) {
  bool did_run = false;
  MessageLoop loop;
  loop.task_runner()->PostTask([&did_run]() {
    did_run = true;
    MessageLoop::GetCurrent()->QuitNow();
  });
  loop.Run();
  EXPECT_TRUE(did_run);
}

TEST(MessageLoop, CanQuitManyTimes) {
  MessageLoop loop;
  loop.QuitNow();
  loop.QuitNow();
  loop.PostQuitTask();
  loop.Run();
  loop.QuitNow();
  loop.QuitNow();
}

class TestMessageLoopHandler : public MessageLoopHandler {
 public:
  TestMessageLoopHandler() {}
  ~TestMessageLoopHandler() override {}

  void clear_ready_count() { ready_count_ = 0; }
  int ready_count() const { return ready_count_; }

  void clear_error_count() { error_count_ = 0; }
  int error_count() const { return error_count_; }

  mx_status_t last_error_result() const { return last_error_result_; }

  // MessageLoopHandler:
  void OnHandleReady(mx_handle_t handle,
                     mx_signals_t pending,
                     uint64_t count) override {
    ready_count_++;
  }
  void OnHandleError(mx_handle_t handle, mx_status_t status) override {
    error_count_++;
    last_error_result_ = status;
  }

 private:
  int ready_count_ = 0;
  int error_count_ = 0;
  mx_status_t last_error_result_ = MX_OK;

  FTL_DISALLOW_COPY_AND_ASSIGN(TestMessageLoopHandler);
};

class QuitOnReadyMessageLoopHandler : public TestMessageLoopHandler {
 public:
  QuitOnReadyMessageLoopHandler() {}
  ~QuitOnReadyMessageLoopHandler() override {}

  void set_message_loop(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }

  void OnHandleReady(mx_handle_t handle,
                     mx_signals_t pending,
                     uint64_t count) override {
    message_loop_->QuitNow();
    TestMessageLoopHandler::OnHandleReady(handle, pending, count);
  }

 private:
  MessageLoop* message_loop_ = nullptr;

  FTL_DISALLOW_COPY_AND_ASSIGN(QuitOnReadyMessageLoopHandler);
};

// Verifies Quit() from OnHandleReady() quits the loop.
TEST(MessageLoop, QuitFromReady) {
  QuitOnReadyMessageLoopHandler handler;
  mx::channel endpoint0;
  mx::channel endpoint1;
  mx::channel::create(0, &endpoint0, &endpoint1);
  mx_status_t rv = endpoint1.write(0, nullptr, 0, nullptr, 0);
  EXPECT_EQ(MX_OK, rv);

  MessageLoop message_loop;
  handler.set_message_loop(&message_loop);
  MessageLoop::HandlerKey key = message_loop.AddHandler(
      &handler, endpoint0.get(), MX_CHANNEL_READABLE, ftl::TimeDelta::Max());
  message_loop.Run();
  EXPECT_EQ(1, handler.ready_count());
  EXPECT_EQ(0, handler.error_count());
  EXPECT_TRUE(message_loop.HasHandler(key));
}

class RemoveOnReadyMessageLoopHandler : public TestMessageLoopHandler {
 public:
  RemoveOnReadyMessageLoopHandler() {}
  ~RemoveOnReadyMessageLoopHandler() override {}

  void set_message_loop(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }

  void set_handler_key(MessageLoop::HandlerKey key) { key_ = key; }

  void OnHandleReady(mx_handle_t handle,
                     mx_signals_t pending,
                     uint64_t count) override {
    EXPECT_TRUE(message_loop_->HasHandler(key_));
    message_loop_->RemoveHandler(key_);
    TestMessageLoopHandler::OnHandleReady(handle, pending, count);
    MessageLoop::GetCurrent()->PostQuitTask();
  }

 private:
  MessageLoop* message_loop_ = nullptr;
  MessageLoop::HandlerKey key_ = 0;

  FTL_DISALLOW_COPY_AND_ASSIGN(RemoveOnReadyMessageLoopHandler);
};

TEST(MessageLoop, HandleReady) {
  RemoveOnReadyMessageLoopHandler handler;
  mx::channel endpoint0;
  mx::channel endpoint1;
  mx::channel::create(0, &endpoint0, &endpoint1);
  mx_status_t rv = endpoint1.write(0, nullptr, 0, nullptr, 0);
  EXPECT_EQ(MX_OK, rv);

  MessageLoop message_loop;
  handler.set_message_loop(&message_loop);
  MessageLoop::HandlerKey key = message_loop.AddHandler(
      &handler, endpoint0.get(), MX_CHANNEL_READABLE, ftl::TimeDelta::Max());
  handler.set_handler_key(key);
  message_loop.Run();
  EXPECT_EQ(1, handler.ready_count());
  EXPECT_EQ(0, handler.error_count());
  EXPECT_FALSE(message_loop.HasHandler(key));
}

TEST(MessageLoop, AfterHandleReadyCallback) {
  RemoveOnReadyMessageLoopHandler handler;
  mx::channel endpoint0;
  mx::channel endpoint1;
  mx::channel::create(0, &endpoint0, &endpoint1);
  mx_status_t rv = endpoint1.write(0, nullptr, 0, nullptr, 0);
  EXPECT_EQ(MX_OK, rv);

  MessageLoop message_loop;
  handler.set_message_loop(&message_loop);
  MessageLoop::HandlerKey key = message_loop.AddHandler(
      &handler, endpoint0.get(), MX_CHANNEL_READABLE, ftl::TimeDelta::Max());
  handler.set_handler_key(key);
  int after_task_callback_count = 0;
  message_loop.SetAfterTaskCallback(
      [&after_task_callback_count] { ++after_task_callback_count; });
  message_loop.Run();
  EXPECT_EQ(1, handler.ready_count());
  EXPECT_EQ(0, handler.error_count());
  EXPECT_EQ(2, after_task_callback_count);
  EXPECT_FALSE(message_loop.HasHandler(key));
}

TEST(MessageLoop, AfterDeadlineExpiredCallback) {
  TestMessageLoopHandler handler;
  mx::channel endpoint0;
  mx::channel endpoint1;
  mx::channel::create(0, &endpoint0, &endpoint1);

  MessageLoop message_loop;
  message_loop.AddHandler(&handler, endpoint0.get(), MX_CHANNEL_READABLE,
                          ftl::TimeDelta::FromMicroseconds(10000));
  message_loop.task_runner()->PostDelayedTask(
      [&message_loop] { message_loop.QuitNow(); },
      ftl::TimeDelta::FromMicroseconds(15000));
  int after_task_callback_count = 0;
  message_loop.SetAfterTaskCallback(
      [&after_task_callback_count] { ++after_task_callback_count; });
  message_loop.Run();
  EXPECT_EQ(2, after_task_callback_count);
}

class QuitOnErrorRunMessageHandler : public TestMessageLoopHandler {
 public:
  QuitOnErrorRunMessageHandler() {}
  ~QuitOnErrorRunMessageHandler() override {}

  void set_message_loop(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }

  void OnHandleError(mx_status_t handle, mx_status_t status) override {
    message_loop_->QuitNow();
    TestMessageLoopHandler::OnHandleError(handle, status);
  }

 private:
  MessageLoop* message_loop_ = nullptr;

  FTL_DISALLOW_COPY_AND_ASSIGN(QuitOnErrorRunMessageHandler);
};

// Verifies Quit() when the deadline is reached works.
// Also ensures that handlers are removed after a timeout occurs.
TEST(MessageLoop, QuitWhenDeadlineExpired) {
  QuitOnErrorRunMessageHandler handler;
  mx::channel endpoint0;
  mx::channel endpoint1;
  mx::channel::create(0, &endpoint0, &endpoint1);

  MessageLoop message_loop;
  handler.set_message_loop(&message_loop);
  MessageLoop::HandlerKey key =
      message_loop.AddHandler(&handler, endpoint0.get(), MX_CHANNEL_READABLE,
                              ftl::TimeDelta::FromMicroseconds(10000));
  message_loop.Run();
  EXPECT_EQ(0, handler.ready_count());
  EXPECT_EQ(1, handler.error_count());
  EXPECT_EQ(MX_ERR_TIMED_OUT, handler.last_error_result());
  EXPECT_FALSE(message_loop.HasHandler(key));
}

// Test that handlers are notified of loop destruction.
TEST(MessageLoop, Destruction) {
  TestMessageLoopHandler handler;
  mx::channel endpoint0;
  mx::channel endpoint1;
  mx::channel::create(0, &endpoint0, &endpoint1);
  {
    MessageLoop message_loop;
    message_loop.AddHandler(&handler, endpoint0.get(), MX_CHANNEL_READABLE,
                            ftl::TimeDelta::Max());
  }
  EXPECT_EQ(1, handler.error_count());
  EXPECT_EQ(MX_ERR_BAD_STATE, handler.last_error_result());
}

class RemoveManyMessageLoopHandler : public TestMessageLoopHandler {
 public:
  RemoveManyMessageLoopHandler() {}
  ~RemoveManyMessageLoopHandler() override {}

  void set_message_loop(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }

  void add_handler_key(MessageLoop::HandlerKey key) { keys_.push_back(key); }

  void OnHandleError(mx_handle_t handle, mx_status_t status) override {
    for (auto key : keys_)
      message_loop_->RemoveHandler(key);
    TestMessageLoopHandler::OnHandleError(handle, status);
  }

 private:
  std::vector<MessageLoop::HandlerKey> keys_;
  MessageLoop* message_loop_ = nullptr;

  FTL_DISALLOW_COPY_AND_ASSIGN(RemoveManyMessageLoopHandler);
};

class ChannelPair {
 public:
  ChannelPair() { mx::channel::create(0, &endpoint0, &endpoint1); }

  mx::channel endpoint0;
  mx::channel endpoint1;
};

// Test that handlers are notified of loop destruction.
TEST(MessageLoop, MultipleHandleDestruction) {
  RemoveManyMessageLoopHandler odd_handler;
  TestMessageLoopHandler even_handler;
  ChannelPair pipe1;
  ChannelPair pipe2;
  ChannelPair pipe3;
  {
    MessageLoop message_loop;
    odd_handler.set_message_loop(&message_loop);
    odd_handler.add_handler_key(
        message_loop.AddHandler(&odd_handler, pipe1.endpoint0.get(),
                                MX_CHANNEL_READABLE, ftl::TimeDelta::Max()));
    message_loop.AddHandler(&even_handler, pipe2.endpoint0.get(),
                            MX_CHANNEL_READABLE, ftl::TimeDelta::Max());
    odd_handler.add_handler_key(
        message_loop.AddHandler(&odd_handler, pipe3.endpoint0.get(),
                                MX_CHANNEL_READABLE, ftl::TimeDelta::Max()));
  }
  EXPECT_EQ(1, odd_handler.error_count());
  EXPECT_EQ(1, even_handler.error_count());
  EXPECT_EQ(MX_ERR_BAD_STATE, odd_handler.last_error_result());
  EXPECT_EQ(MX_ERR_BAD_STATE, even_handler.last_error_result());
}

class AddHandlerOnErrorHandler : public TestMessageLoopHandler {
 public:
  AddHandlerOnErrorHandler() {}
  ~AddHandlerOnErrorHandler() override {}

  void set_message_loop(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }

  void OnHandleError(mx_handle_t handle, mx_status_t status) override {
    message_loop_->AddHandler(this, handle, MX_CHANNEL_READABLE,
                              ftl::TimeDelta::Max());
    TestMessageLoopHandler::OnHandleError(handle, status);
  }

 private:
  MessageLoop* message_loop_ = nullptr;

  FTL_DISALLOW_COPY_AND_ASSIGN(AddHandlerOnErrorHandler);
};

// Ensures that the MessageLoop doesn't get into infinite loops if
// new handlers are added while canceling handlers during MessageLoop
// destruction.
TEST(MessageLoop, AddHandlerOnError) {
  AddHandlerOnErrorHandler handler;
  mx::channel endpoint0;
  mx::channel endpoint1;
  mx::channel::create(0, &endpoint0, &endpoint1);
  {
    MessageLoop message_loop;
    handler.set_message_loop(&message_loop);
    message_loop.AddHandler(&handler, endpoint0.get(), MX_CHANNEL_READABLE,
                            ftl::TimeDelta::Max());
  }
  EXPECT_EQ(1, handler.error_count());
  EXPECT_EQ(MX_ERR_BAD_STATE, handler.last_error_result());
}

// Tests that waiting on files in a MessageLoop works.
TEST(MessageLoop, FDWaiter) {
  // Create an event and an FD that reflects that event. The fd
  // shares ownership of the event.
  mx::event fdevent;
  EXPECT_EQ(mx::event::create(0u, &fdevent), MX_OK);
  ftl::UniqueFD fd(
      mxio_handle_fd(fdevent.get(), MX_USER_SIGNAL_0, 0, /*shared=*/true));
  EXPECT_TRUE(fd.is_valid());

  FDWaiter waiter;
  bool callback_ran = false;
  {
    MessageLoop message_loop;
    std::thread thread([&fdevent]() {
      // Poke the fdevent, which pokes the fd.
      EXPECT_EQ(fdevent.signal(0u, MX_USER_SIGNAL_0), MX_OK);
    });
    auto callback = [&callback_ran, &message_loop](mx_status_t success,
                                                   uint32_t events) {
      EXPECT_EQ(success, MX_OK);
      EXPECT_EQ(events, static_cast<uint32_t>(POLLIN));
      callback_ran = true;
      message_loop.QuitNow();
    };
    EXPECT_TRUE(waiter.Wait(callback, fd.get(), POLLIN));
    message_loop.Run();
    thread.join();
  }

  EXPECT_TRUE(callback_ran);
}

}  // namespace
}  // namespace mtl
