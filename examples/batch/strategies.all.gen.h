// GENERATED FILE - DO NOT EDIT
// Sources: examples/batch/strategies/alpha.yaml, examples/batch/strategies/beta.yaml
#ifndef CXGN_EXAMPLES_BATCH_STRATEGIES_ALL_GEN_H
#define CXGN_EXAMPLES_BATCH_STRATEGIES_ALL_GEN_H

#include <stddef.h>
#include <stdbool.h>
#include <cxgn/macros.h>
#include "strategy.h"

/* -- Entry: strategies/alpha -- Source: examples/batch/strategies/alpha.yaml */
static const StrategyPreset strategies_alpha_config = {
    .name = "alpha",
    .threshold = 10
};

/* -- Entry: strategies/beta -- Source: examples/batch/strategies/beta.yaml */
static const StrategyPreset strategies_beta_config = {
    .name = "beta",
    .threshold = 20
};


/* == Config registry == */
typedef struct {
    const char* key;
    const StrategyPreset* config;
} cxgn_map_entry_t;

typedef struct Config {
    const cxgn_map_entry_t* entries;
    size_t count;
} Config;

static const cxgn_map_entry_t _config_entries[] = {
    {"strategies/alpha", &strategies_alpha_config},
    {"strategies/beta", &strategies_beta_config},
};

static const Config config = {
    .entries = _config_entries,
    .count = 2,
};

#endif /* CXGN_EXAMPLES_BATCH_STRATEGIES_ALL_GEN_H */
