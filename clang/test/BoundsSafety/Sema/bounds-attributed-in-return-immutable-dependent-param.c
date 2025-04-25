// TODO: We should get the same diagnostics with/without return_size (rdar://138982703)

// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify=expected,rs -fbounds-safety-bringup-missing-checks=return_size %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -x objective-c -fbounds-attributes-objc-experimental -verify=expected,rs -fbounds-safety-bringup-missing-checks=return_size %s
#include "bounds-attributed-in-return-immutable-dependent-param-common.inc"
