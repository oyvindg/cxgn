#pragma once
#include "Vec3.h"

/** Segment length and joint angle limits (degrees) for one leg joint. */
typedef struct Joint {
    float length;
    float min;
    float max;
} Joint;

/**
 * Static configuration for a single hexapod leg.
 *
 * Positions are in the body frame (mm).
 * Lengths are segment lengths (mm).
 * Angles are in degrees.
 */
typedef struct HexapodLeg {
    int index;
    Vec3 basePosition;
    Vec3 restPosition;
    Joint coxa;
    Joint femur;
    Joint tibia;
} HexapodLeg;
