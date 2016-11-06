// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/mtl/vmo/strings.h"
#include "gtest/gtest.h"

namespace mtl {
namespace {

TEST(VmoStrings, ShortString) {
  const std::string hello_string = "Hello, world.";
  mx::vmo hello_buffer;
  EXPECT_TRUE(VmoFromString(hello_string, &hello_buffer));
  std::string hello_out;
  EXPECT_TRUE(StringFromVmo(std::move(hello_buffer), &hello_out));
  EXPECT_EQ(hello_string, hello_out);
}

TEST(VmoStrings, EmptyString) {
  const std::string hello_string = "";
  mx::vmo hello_buffer;
  EXPECT_TRUE(VmoFromString(hello_string, &hello_buffer));
  std::string hello_out;
  EXPECT_TRUE(StringFromVmo(std::move(hello_buffer), &hello_out));
  EXPECT_EQ(hello_string, hello_out);
}

TEST(VmoStrings, BinaryString) {
  std::string binary_string('\0', 10);
  for (size_t i = 0; i < binary_string.size(); i++) {
    binary_string[i] = (char)i;
  }
  mx::vmo binary_buffer;
  EXPECT_TRUE(VmoFromString(binary_string, &binary_buffer));
  std::string binary_out;
  EXPECT_TRUE(StringFromVmo(std::move(binary_buffer), &binary_out));
  EXPECT_EQ(binary_string, binary_out);
}

}  // namespace
}  // namespace mtl
