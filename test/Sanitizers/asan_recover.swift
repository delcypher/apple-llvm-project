// REQUIRES: executable_test
// REQUIRES: asan_runtime

// Check with recovery instrumentation and runtime option to continue execution.
// RUN: %target-swiftc_driver %s -target %sanitizers-target-triple -g -sanitize=address -sanitize-recover=address -import-objc-header %S/asan_interface.h -o %t_asan_recover
// RUN: %target-codesign %t_asan_recover
// RUN: env %env-ASAN_OPTIONS=halt_on_error=0 %target-run %t_asan_recover > %t_asan_recover.stdout 2> %t_asan_recover.stderr
// RUN: %FileCheck --check-prefixes=CHECK-COMMON-STDERR,CHECK-RECOVER-STDERR -input-file=%t_asan_recover.stderr %s
// RUN: %FileCheck --check-prefixes=CHECK-COMMON-STDOUT,CHECK-RECOVER-STDOUT -input-file=%t_asan_recover.stdout %s

// Check with recovery instrumentation and but without runtime option to continue execution.
// RUN: not env %env-ASAN_OPTIONS=abort_on_error=0,halt_on_error=1 %target-run %t_asan_recover > %t_asan_no_runtime_recover.stdout 2> %t_asan_no_runtime_recover.stderr
// RUN: %FileCheck --check-prefixes=CHECK-COMMON-STDERR -input-file=%t_asan_no_runtime_recover.stderr %s
// RUN: %FileCheck --check-prefixes=CHECK-COMMON-STDOUT,CHECK-NO-RECOVER-STDOUT -input-file=%t_asan_no_runtime_recover.stdout %s

// Check without recovery instrumentation that error recovery does not happen.
// RUN: %target-swiftc_driver %s -target %sanitizers-target-triple -g -sanitize=address -import-objc-header %S/asan_interface.h -o %t_asan_no_recover
// RUN: %target-codesign %t_asan_no_recover
// RUN: not env %env-ASAN_OPTIONS=abort_on_error=0,halt_on_error=0 %target-run %t_asan_no_recover > %t_asan_no_recover.stdout 2> %t_asan_no_recover.stderr
// RUN: %FileCheck --check-prefixes=CHECK-COMMON-STDERR -input-file=%t_asan_no_recover.stderr %s
// RUN: %FileCheck --check-prefixes=CHECK-COMMON-STDOUT,CHECK-NO-RECOVER-STDOUT -input-file=%t_asan_no_recover.stdout %s

// FIXME: We need this so we can flush stdout but this won't
// work on other Platforms (e.g. Windows).
#if os(Linux)
    import Glibc
#else
    import Darwin.C
#endif

// CHECK-COMMON-STDOUT: START
print("START")
fflush(stdout)

let size:Int = 128;

var x = UnsafeMutablePointer<UInt8>.allocate(capacity: size)
x.initialize(repeating: 0, count: size)

__asan_poison_memory_region(UnsafeMutableRawPointer(x), size)

// First error
// NOTE: Reading this way seems to generate a read via instrumentation which is
// what we want to test for and why that stackframe number we check for is 0.
// CHECK-COMMON-STDERR: AddressSanitizer: use-after-poison
// CHECK-COMMON-STDERR: #0 0x{{.+}} in main asan_recover.swift:[[@LINE+1]]
print("Read first element:\(x.advanced(by: 0).pointee)")
fflush(stdout)
// CHECK-RECOVER-STDOUT: Read first element:0

// Second error
// CHECK-RECOVER-STDERR: AddressSanitizer: use-after-poison
// CHECK-RECOVER-STDERR: #0 0x{{.+}} in main asan_recover.swift:[[@LINE+1]]
print("Read second element:\(x.advanced(by: 1).pointee)")
fflush(stdout)
// CHECK-RECOVER-STDOUT: Read second element:0

__asan_unpoison_memory_region(UnsafeMutableRawPointer(x), size)

x.deallocate();
// CHECK-NO-RECOVER-STDOUT-NOT: DONE
// CHECK-RECOVER-STDOUT: DONE
print("DONE")
fflush(stdout)
