// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify %s
#include <bounds_safety_soft_traps.h>

#ifndef __CLANG_BOUNDS_SAFETY_SOFT_TRAP_API_VERSION
#error macro definition missing
#endif

#if __CLANG_BOUNDS_SAFETY_SOFT_TRAP_API_VERSION > 0
#error API version bumped without updating test
#endif

// We should get error diagnostics if there's a function signature mismatch
// between the header and the declarations below.
void __bounds_safety_soft_trap_s(const char *reason) {

}

void __bounds_safety_soft_trap_c(uint16_t reason_code) {
    
}

// expected-no-diagnostics
