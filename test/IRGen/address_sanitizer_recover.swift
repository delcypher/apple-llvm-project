// RUN: %target-swift-frontend -emit-ir -sanitize=address -sanitize-recover=address %s | %FileCheck %s -check-prefix=ASAN_RECOVER
// RUN: %target-swift-frontend -emit-ir -sanitize=address  %s | %FileCheck %s -check-prefix=ASAN_NO_RECOVER

// ASAN_RECOVER: declare void @__asan_loadN_noabort(
// ASAN_NO_RECOVER: declare void @__asan_loadN(

func test() { }
