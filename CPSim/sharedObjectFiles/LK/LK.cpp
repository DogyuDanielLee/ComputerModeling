// Team4_LK.cpp - Team 4 LK (the engine needs this file's name on a line before sim_main)
#include "LK.h"

void sim_main()
{
  /********************************************/
  /*   Team 4 - Lane Keeping (PD controller)  */
  /********************************************/
  // Inputs  (from TORCS): rtU->read1 = lateral error (signed), rtU->read2 = LK enable signal
  // Outputs (to TORCS)  : rtY->write3 = steering for read1 < 0, rtY->write4 = steering for read1 > 0
  //
  // The skeleton/slide controller is P-only: steer = |read1| clamped to 10000.
  // Sampled every LK period, a P-only loop overshoots and weaves, which costs speed.
  // We add a derivative (damping) term on the change of the error between two LK jobs.
  //
  // NOTE: the engine compiles only the body of this function, so all tuning constants
  // and state must stay inside it (static locals keep their value between jobs).

  // ---- tuning ----
  const double KP           = 1.0;      // proportional gain (slide value)
  const double LK_PERIOD_MS = 110.0;    // must match the LK period in the design
  const double TD_MS        = 250.0;    // derivative time; 0 -> exactly the slide's P-only controller
  const double STEER_MAX    = 10000.0;  // steering clamp (slide value; skeleton had 1000)
  const double ENABLE_LEVEL = 5000.0;   // LK active only while read2 > this (skeleton logic)

  const double KD = TD_MS / LK_PERIOD_MS;  // derivative gain per LK job

  const int    STALE_JOBS   = 2;        // same input this many LK jobs in a row -> treat data as stale

  static double prev_error = 0.0;
  static int has_prev = 0;
  static double last_raw = 0.0;
  static int same_count = 0;

  double error = rtU->read1;
  double steer = 0.0;

  // Stale-data guard: TORCS sends fresh values every ~30 ms, so a real moving car never gives
  // the exact same lateral error twice in a row 110 ms apart. Seen on the real setup: the
  // values froze for ~3 s while the car kept moving; holding the last steering command
  // put it into the wall. While the input is frozen, steer straight.
  if (error == last_raw && error != 0.0)
    same_count++;
  else
    same_count = 0;
  last_raw = error;
  int stale = (same_count >= STALE_JOBS);

  if (rtU->read2 <= ENABLE_LEVEL || stale)
  {
    // LK disabled or input stale: no steering, and forget the old error so restarting does not kick
    has_prev = 0;
  }
  else
  {
    double derivative = has_prev ? (error - prev_error) : 0.0;
    steer = KP * error + KD * derivative;
    prev_error = error;
    has_prev = 1;
  }

  if (steer > STEER_MAX)  steer = STEER_MAX;
  if (steer < -STEER_MAX) steer = -STEER_MAX;

  // Same sign convention as the original: positive error -> write4, negative error -> write3
  rtDW->w3 = (steer < 0.0) ? -steer : 0.0;
  rtDW->w4 = (steer > 0.0) ?  steer : 0.0;

  rtY->write3 = rtDW->w3;
  rtY->write4 = rtDW->w4;
}
