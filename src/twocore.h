// twocore.h
// The second processor core. The deck's own code runs on core 0, and core 1 has no work of
// its own. For a heavy drawing job, core 0 gives core 1 one part of the picture rows and does
// the other part itself, so the job takes about half the time.
// A job must only write to its own rows, and must not use data that the other part changes.
#pragma once

#include <Arduino.h>

#ifdef DECK_PROF
static uint32_t shareOwn = 0, shareOther = 0, shareWait = 0;   // frame timing: core 0's part, core 1's part, core 0's wait for core 1
#endif

#ifdef ARDUINO_ARCH_RP2040
#include <pico/sync.h>

namespace twocore {
static semaphore_t go, done;
static void (*volatile job)(void *, int) = nullptr;
static void *volatile arg = nullptr;
static volatile bool ready = false;            // core 1 waits for work
static volatile uint32_t took = 0;             // how long core 1's part took, microseconds
}

// The board package runs these two on core 1.
void setup1() {
  sem_init(&twocore::go, 0, 1);
  sem_init(&twocore::done, 0, 1);
  twocore::ready = true;
}

void loop1() {
  sem_acquire_blocking(&twocore::go);          // sleeps until there is a job
  uint32_t t0 = micros();
  twocore::job(twocore::arg, 1);
  twocore::took = micros() - t0;
  sem_release(&twocore::done);
}
#endif

// Run job(arg, 0) on this core and job(arg, 1) on the other core at the same time. Returns
// when both are done. On the PC simulator, and until core 1 is ready, both parts run here.
// The result is how many microseconds part 0 took more than part 1 (0 if they ran here).
static int onBothCores(void (*job)(void *, int), void *arg) {
#ifdef ARDUINO_ARCH_RP2040
  if (twocore::ready) {
    twocore::job = job;
    twocore::arg = arg;
    sem_release(&twocore::go);
    uint32_t t0 = micros();
    job(arg, 0);
    uint32_t t1 = micros();
    // core 1 gives no answer in 3 s: something is wrong with it, so work without it from now on
    if (!sem_acquire_timeout_ms(&twocore::done, 3000)) {
      twocore::ready = false;
      return 0;
    }
#ifdef DECK_PROF
    shareOwn += t1 - t0;
    shareOther += twocore::took;
    shareWait += micros() - t1;
#endif
    return (int)(t1 - t0) - (int)twocore::took;
  }
#endif
  job(arg, 0);
  job(arg, 1);
  return 0;
}

// Move a split row a little towards an even share of the work. `more` is the result of
// onBothCores for a job where core 0 had the rows above the split.
static inline float evenShare(float split, int more, float lo, float hi) {
  return constrain(split - more * 0.004f, lo, hi);
}
