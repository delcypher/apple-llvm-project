// RUN: not %swiftc_driver -driver-print-jobs -sanitize=address -sanitize-recover=foo %s 2>&1 | %FileCheck -check-prefix=SAN_RECOVER_INVALID_ARG %s
// RUN: not %swiftc_driver -driver-print-jobs -sanitize=address -sanitize-recover=thread %s 2>&1 | %FileCheck -check-prefix=SAN_RECOVER_UNSUPPORTED_ARG %s
// RUN: not %swiftc_driver -driver-print-jobs -sanitize-recover=address %s 2>&1 | %FileCheck -check-prefix=SAN_RECOVER_MISSING_INSTRUMENTATION_OPTION %s
// RUN: %swiftc_driver -driver-print-jobs -sanitize=address -sanitize-recover=address %s 2>&1 | %FileCheck -check-prefix=ASAN_WITH_RECOVER  %s
// RUN: %swiftc_driver -driver-print-jobs -sanitize=address -sanitize-recover=address %s 2>&1 | %FileCheck -check-prefix=ASAN_WITHOUT_RECOVER --implicit-check-not='-sanitize-recover=address' %s

// SAN_RECOVER_INVALID_ARG: unsupported argument 'foo' to option '-sanitize-recover='
// SAN_RECOVER_UNSUPPORTED_ARG: unsupported argument 'thread' to option '-sanitize-recover='
// SAN_RECOVER_MISSING_INSTRUMENTATION_OPTION: option '-sanitize-recover=address' requires 'address' sanitizer to be enabled. Use -sanitize=address to enable the sanitizer

// ASAN_WITH_RECOVER: swift
// ASAN_WITHOUT_RECOVER-DAG: -sanitize=address
// ASAN_WITHOUT_RECOVER-DAG: -sanitize-recover=address

// ASAN_WITHOUT_RECOVER: swift
// ASAN_WITH_RECOVER: -sanitize=address
