// GENERATED FILE - DO NOT EDIT
// Sources: fixtures/batch/alpha.yaml, fixtures/batch/beta.yaml
#ifndef CXGN_FIXTURES____CLI_PLAN_BATCH_GEN_H
#define CXGN_FIXTURES____CLI_PLAN_BATCH_GEN_H

#include <stddef.h>
#include <stdbool.h>
#include "fixtures/simple.h"

/* -- Entry: alpha -- Source: fixtures/batch/alpha.yaml */
static const SimpleConfig alpha_config = {
    .timeout = 10,
    .name = "alpha",
    .enabled = true
};

/* -- Entry: beta -- Source: fixtures/batch/beta.yaml */
static const SimpleConfig beta_config = {
    .timeout = 20,
    .name = "beta",
    .enabled = false
};


/* == Config registry == */
typedef struct {
    const char* key;
    const SimpleConfig* config;
} cxgn_map_entry_t;

typedef struct cli_plan_map_registry_t {
    const cxgn_map_entry_t* entries;
    size_t count;
} cli_plan_map_registry_t;

static const cxgn_map_entry_t _cli_plan_map_entries[] = {
    {"alpha", &alpha_config},
    {"beta", &beta_config},
};

static const cli_plan_map_registry_t cli_plan_map = {
    .entries = _cli_plan_map_entries,
    .count = 2,
};

#endif /* CXGN_FIXTURES____CLI_PLAN_BATCH_GEN_H */
