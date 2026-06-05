#ifndef CXGN_MACROS_H
#define CXGN_MACROS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Defines a read-only array view for generated config data. */
#define CXGN_ARRAY_TYPEDEF(T, Name) typedef struct { const T* data; size_t count; } Name;

/* Defines an optional value with an explicit presence flag. */
#define CXGN_OPTIONAL_TYPEDEF(T, Name) typedef struct { T value; bool has_value; } Name;

#ifdef __cplusplus
}
#endif

#endif /* CXGN_MACROS_H */
