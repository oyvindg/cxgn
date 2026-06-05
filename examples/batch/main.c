#include "strategies.all.gen.h"
#include <stdio.h>

int main(void) {
    for (size_t i = 0; i < config.count; i++) {
        const StrategyPreset* preset = config.entries[i].config;
        printf("%s: %s threshold=%d\n",
               config.entries[i].key,
               preset->name,
               preset->threshold);
    }
    return 0;
}
