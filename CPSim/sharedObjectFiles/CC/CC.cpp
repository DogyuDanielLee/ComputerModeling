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
  //   TARGET_OFFSET = 400000 is the slide's baseline (CC speed 40 km/h).
  //   TARGET_OFFSET = 4000 (skeleton) puts the target far above what the car can reach,
  //   so CC asks for full acceleration the whole lap.
  // Safety: if the lateral error is far beyond what LK can correct (steering saturated),
  //   the car is running wide in a corner, so lift off until it is back near the lane.
  //
  // NOTE: the engine compiles only the body of this function, so all tuning constants
  // and state must stay inside it (static locals keep their value between jobs).

  // ---- tuning ----
  const int    TARGET_OFFSET = 4000;     // 400000 = 40 km/h baseline, 4000 = flat out
  const int    LIFT_ENABLED  = 1;        // 0 -> exactly the original CC logic
  const double LIFT_ON       = 15000.0;  // |lateral error| that starts the lift (LK clamp is 10000)
  const double LIFT_OFF      = 8000.0;   // |lateral error| that ends it (hysteresis)
  const double CC_ENABLE     = 6000.0;   // CC active only while CC_TRIGGER > this (skeleton logic)
  const double LK_ENABLE     = 5000.0;   // LK active only while rtU->read2 > this

  static int lifting = 0;

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
