#pragma once

/**
 * Tiny batch schema. cxgn ignores comments before and inside schema structs.
 */
typedef struct StrategyPreset {
    const char* name; /* human-readable preset name */

    /*
     * Threshold is intentionally simple for the batch example.
     */
    int threshold; // Trigger level.
} StrategyPreset;
