// Team4_CC.cpp - Team 4 CC (the engine needs this file's name on a line before sim_main)
#include "CC.h"

void sim_main()
{
  /********************************************/
  /*   Team 4 - Cruise Control                */
  /********************************************/
  // Inputs  (from TORCS): ACCEL_VALUE, TARGET_SPEED, SPEED, CC_TRIGGER
  // Outputs (to TORCS)  : BRAKE, ACCEL
  //
  // Speed target: target = TARGET_SPEED / 100 - TARGET_OFFSET (original formula).
  //   TORCS sends TARGET_SPEED = 80000000 and SPEED in units of 1/10000 km/h, so
  //   TARGET_OFFSET = 4000 (provided code) -> ~80 km/h, 400000 (slide baseline) -> 40 km/h,
  //   and a negative offset goes faster (-200000 -> 100 km/h).
  //   Tested on E-Track 5: 120 and 140 km/h complete the race, 180 and 200 km/h spin out.
  //   We keep 100 km/h for margin.
  //
  // NOTE: the engine compiles only the body of this function, so all tuning constants
  // and state must stay inside it (static locals keep their value between jobs).

  // ---- tuning ----
  const int    TARGET_OFFSET = -200000;  // target = 80 - (offset / 10000) km/h -> 100 km/h
  const double CC_ENABLE     = 6000.0;   // CC active only while CC_TRIGGER > this (skeleton logic)

  const int    STALE_JOBS    = 2;        // same speed this many CC jobs in a row while moving -> stale
  const int    MOVING_SPEED  = 100000;   // only check for stale data above ~10 km/h (start of race is still)

  static int last_speed = 0;
  static int same_count = 0;

  unsigned int read1;
  int read2;
  int read3;
  int read4;
  int write5;
  int write6;

  /********************************************/
  /***************** Data Read ****************/
  /********************************************/
  read1 = *CC_Recv_ACCEL_VALUE;
  read2 = *CC_Recv_TARGET_SPEED;
  read3 = *CC_Recv_SPEED;
  read4 = *CC_Recv_CC_TRIGGER;

  /********************************************/
  /***************** Processing ***************/
  /********************************************/
  write5 = 0;
  write6 = 0;
  read2 = read2 / 100 - TARGET_OFFSET;

  // Stale-data guard: a moving car does not report the exact same speed twice 110 ms apart.
  // If TORCS values freeze (seen on the testbed, 1.5-3 s), we are driving blind: release the throttle.
  if (read3 == last_speed && (read3 > MOVING_SPEED || read3 < -MOVING_SPEED))
    same_count++;
  else
    same_count = 0;
  last_speed = read3;
  int stale = (same_count >= STALE_JOBS);

  if (read4 > CC_ENABLE)
  {
    write6 = read1;

    if (read3 - read2 < 0)
      write6 = read2;
    else if (read3 > read2)
      write6 = 0;

    if (stale)
      write6 = 0;
  }

  /********************************************/
  /***************** Data Write ***************/
  /********************************************/
  *CC_Send_BRAKE = write5;
  *CC_Send_ACCEL = write6;
}
