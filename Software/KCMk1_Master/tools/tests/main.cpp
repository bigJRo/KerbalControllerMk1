// Selftest entry: every stage's tests, in order.
#include "harness.h"

void runStage1();
void runStage2();
void runStage3();
void runStage4();
void runStage5();

int main() {
  runStage1();
  runStage2();
  runStage3();
  runStage4();
  runStage5();
  int f = g_failsRef(), c = g_checksRef();
  printf(f ? "selftest: %d of %d checks FAILED\n" : "selftest: all %d checks passed\n", f ? f : c, c);
  return f != 0;
}
