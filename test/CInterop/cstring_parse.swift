// Note: this test intentionally uses a private module cache.
//
// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -typecheck -verify -module-cache-path %t/clang-module-cache %s
// RUN: ls -lR %t/clang-module-cache | %FileCheck %s
// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -enable-cxx-interop -typecheck -verify -module-cache-path %t/clang-module-cache %s

// CHECK: cfuncs{{.*}}.pcm

import cfuncs

func test_puts(_ s: String) {
  _ = puts(s) + (32 as Int32)
}
