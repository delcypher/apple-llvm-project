// RUN: %clang_cc1 -verify=no_bounds_safety %s
// RUN: %clang_cc1 -fbounds-safety -verify %s
#include <ptrcheck.h>

#define __bs_unique_trap __attribute__((unique_traps("bounds-safety")))

// =============================================================================
// Correct use of the attribute
// =============================================================================
#if __has_ptrcheck

// Accepted on function declaration or definition
void attr_accepted_0(void) __bs_unique_trap;
void attr_accepted_0(void) __bs_unique_trap {}


void attr_accepted_1(void) __attribute__((unique_traps("bounds-safety", "bounds-safety"))) {}

#else

// no_bounds_safety-warning@+1{{'unique_traps' attribute ignored}}
void attr_rejected_0(void) __bs_unique_trap {}

#endif

// =============================================================================
// Incorrect use of the attribute
// =============================================================================


// Rejected on params
// no_bounds_safety-warning@+2{{'unique_traps' attribute ignored}}
// expected-error@+1{{'unique_traps' attribute only applies to functions}}
void attr_rejected_1(int* __bs_unique_trap p) {}

void attr_rejected_2(void) {
    // no_bounds_safety-warning@+2{{'unique_traps' attribute ignored}}
    // expected-error@+1{{'unique_traps' attribute only applies to functions}}
    int* __bs_unique_trap q;
}

// no_bounds_safety-warning@+2{{'unique_traps' attribute ignored}}
// expected-error@+1{{unique_traps' attribute does not accept "bad_value" as argument}}
void attr_rejected_3(void) __attribute__((unique_traps("bad_value")));

// no_bounds_safety-warning@+2{{'unique_traps' attribute ignored}}
// expected-error@+1{{'unique_traps' attribute does not accept "" as argument}}
void attr_rejected_4(void) __attribute__((unique_traps("")));

// no_bounds_safety-error@+2{{expected string literal as argument of 'unique_traps' attribute}}
// expected-error@+1{{expected string literal as argument of 'unique_traps' attribute}}
void attr_rejected_5(void) __attribute__((unique_traps(0)));

// no_bounds_safety-warning@+2{{'unique_traps' attribute ignored}}
// expected-error@+1{{'unique_traps' attribute takes at least 1 argument}}
void attr_accepted_6(void) __attribute__((unique_traps));

// no_bounds_safety-warning@+2{{'unique_traps' attribute ignored}}
// expected-error@+1{{'unique_traps' attribute takes at least 1 argument}}
void attr_accepted_7(void) __attribute__((unique_traps()));

// no_bounds_safety-warning@+2{{'unique_traps' attribute ignored}}
// expected-error@+1{{'unique_traps' attribute does not accept "bad_value" as argument}}
void attr_accepted_8(void) __attribute__((unique_traps("bounds-safety", "bad_value")));

void* identifier;
// no_bounds_safety-warning@+3{{'unique_traps' attribute ignored}}
// expected-error@+2{{'unique_traps' attribute requires a string}}
// expected-error@+1{{'unique_traps' attribute does not accept "identifier" as argument}}
void attr_accepted_9(void) __attribute__((unique_traps(identifier)));
