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
  //   TARGET_OFFSET = 4000 (skeleton) -> target 796000 = ~80 km/h (lap ~3:06 measured),
  //   TARGET_OFFSET = 400000 (slide baseline) -> 40 km/h, and a negative offset goes faster.
  // Safety: if the lateral error is far beyond what LK can correct (steering saturated),
  //   the car is running wide in a corner, so lift off until it is back near the lane.
  //
  // NOTE: the engine compiles only the body of this function, so all tuning constants
  // and state must stay inside it (static locals keep their value between jobs).

  // ---- tuning ----
  const int    TARGET_OFFSET = -200000;  // target = 80 - (offset / 10000) km/h -> 100 km/h
  const int    LIFT_ENABLED  = 1;        // 0 -> exactly the original CC logic
  const double LIFT_ON       = 15000.0;  // |lateral error| that starts the lift (LK clamp is 10000)
  const double LIFT_OFF      = 8000.0;   // |lateral error| that ends it (hysteresis)
  const double CC_ENABLE     = 6000.0;   // CC active only while CC_TRIGGER > this (skeleton logic)
  const double LK_ENABLE     = 5000.0;   // LK active only while rtU->read2 > this

  const int    STALE_JOBS    = 2;        // same speed this many CC jobs in a row while moving -> stale
  const int    MOVING_SPEED  = 100000;   // only check for stale data above ~10 km/h (start of race is still)

  static int lifting = 0;
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

  // Stale-data guard: a moving car never reports the exact same speed twice 110 ms apart.
  // If TORCS values freeze (seen on the real setup, ~3 s), we are driving blind: lift off.
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

    double lateral = rtU->read1;
    if (lateral < 0.0) lateral = -lateral;

    if (LIFT_ENABLED && rtU->read2 > LK_ENABLE)
    {
      if (lifting && lateral < LIFT_OFF)
        lifting = 0;
      else if (!lifting && lateral > LIFT_ON)
        lifting = 1;

      if (lifting)
        write6 = 0;
    }
    else
    {
      lifting = 0;
    }

    if (stale)
      write6 = 0;
  }
  else
  {
    lifting = 0;
  }

  /********************************************/
  /***************** Data Write ***************/
  /********************************************/
  *CC_Send_BRAKE = write5;
  *CC_Send_ACCEL = write6;
}
