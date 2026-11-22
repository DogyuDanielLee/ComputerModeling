// Team4_LK.cpp - Team 4 LK (the engine needs this file's name on a line before sim_main)
#include "LK.h"

void sim_main()
{
  /********************************************/
  /*   Team 4 - Lane Keeping (PID controller) */
  /********************************************/
  // Inputs  (from TORCS): rtU->read1 = lateral error (signed), rtU->read2 = LK enable signal
  // Outputs (to TORCS)  : rtY->write3 = steering for read1 < 0, rtY->write4 = steering for read1 > 0
  //
  // Gains were tuned on a model identified from real TORCS runs (E-Track 5, LK period 110 ms):
  //   dL[k] = 0.39 dL[k-1] + 0.05 dL[k-2] - 0.32 u[k-1] + 0.01 u[k-2] + 0.10 u[k-3]  (R^2 = 0.83)
  // where L is the lateral error and u the steering command, one step per LK job.
  // The slide's P-only controller (gain 1) and our first PD version (D gain 2.27) both make the
  // steering flip direction ~3 times per second (wiggling). Lower P with a small D and I gives
  // a smaller lateral error with ~0.6 direction changes per second, and stays stable if the car
  // reacts up to 2x more strongly to steering (higher speed).
  //
  // NOTE: the engine compiles only the body of this function, so all tuning constants
  // and state must stay inside it (static locals keep their value between jobs).

  // ---- tuning (per LK job) ----
  const double KP           = 0.7;      // proportional gain (slide value: 1.0)
  const double KD           = 0.3;      // gain on the change of the error since the last LK job
  const double KI           = 0.01;     // gain on the summed error (removes the offset in long corners)
  const double I_MAX        = 3000.0;   // max steering from the I term (anti-windup)
  const double STEER_MAX    = 10000.0;  // steering clamp (slide value)
  const double ENABLE_LEVEL = 5000.0;   // LK active only while read2 > this (skeleton logic)
  const int    STALE_JOBS   = 2;        // same input this many LK jobs in a row -> treat data as stale

  static double prev_error = 0.0;
  static double integral = 0.0;         // in steering units (KI already applied)
  static int has_prev = 0;
  static double last_raw = 0.0;
  static int same_count = 0;

  double error = rtU->read1;
  double steer = 0.0;

  // Stale-data guard: TORCS sends fresh values every ~30 ms, so a moving car never gives the
  // exact same lateral error twice in a row 110 ms apart. Seen on the real setup: values froze
  // for 1.5-3 s while the car kept moving; holding the last steering command put it into the
  // wall. While the input is frozen, steer straight.
  if (error == last_raw && error != 0.0)
    same_count++;
  else
    same_count = 0;
  last_raw = error;
  int stale = (same_count >= STALE_JOBS);

  if (rtU->read2 <= ENABLE_LEVEL || stale)
  {
    // LK disabled or input stale: no steering, and reset the state so restarting does not kick
    has_prev = 0;
    integral = 0.0;
  }
  else
  {
    double derivative = has_prev ? (error - prev_error) : 0.0;

    integral += KI * error;
    if (integral > I_MAX)  integral = I_MAX;
    if (integral < -I_MAX) integral = -I_MAX;

    steer = KP * error + KD * derivative + integral;
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
