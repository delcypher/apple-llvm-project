// REQUIRES: bounds-safety-bringup-missing-checks-on-by-default
// TODO: We should get the same diagnostics with/without compound_literal_init (rdar://138982703)
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify=expected,both %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -x objective-c -fbounds-attributes-objc-experimental -verify=expected,both %s
#include "compound-literal-counted_by-common.inc"
