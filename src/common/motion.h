#pragma once
#include <Arduino.h>

// Quarter-turn detection on the on-board QMI8658 IMU.
// Shared board service: reports orientation, apps decide what it means.
//
// Gravity projected onto the screen plane gives the angle the board is held
// at. That angle is snapped to one of four quadrants; when a new quadrant has
// been held steadily for TILT_STABLE_MS the device has been turned 90 degrees
// and the chronometer starts over -- the same gesture as flipping an hourglass.

bool motionBegin();
bool motionAvailable();

// Poll from the main loop. Returns true exactly once per settled quarter turn.
bool motionPoll();

// 0..3, or -1 while the board is lying flat and orientation is meaningless.
int8_t motionOrientation();

// Last accelerometer sample in g. False if the IMU never came up or has not
// produced a sample yet.
bool motionAccel(float *x, float *y, float *z);
