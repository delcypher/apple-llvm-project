#include <stdint.h>

__attribute__((noinline, optnone)) void test_breakpoint(void) {}
__attribute__((noinline, optnone)) void test_breakpoint_2(void) {}

__attribute__((noinline, optnone)) int shift(void) { return 33; }

int main() {
  uint32_t x = 0;

  x = x << shift(); // first ubsan issue
  x = x << shift(); // second ubsan issue
  test_breakpoint();
  x = x << shift(); // third ubsan issue
  test_breakpoint_2();
  x = x << shift(); // fourth ubsan issue

  return 0;
}
