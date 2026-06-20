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

/* Defines a keyed-array view for a YAML mapping with arbitrary keys. A YAML
 * `key: value` mapping generates an array of T, with each entry's FIRST field
 * set to the key and the remaining field(s) from the value: a scalar value
 * fills T's second field; an object value spreads into T's fields. Same C
 * representation as an array view. */
#define CXGN_MAP_TYPEDEF(T, Name) typedef struct { const T* data; size_t count; } Name;

#ifdef __cplusplus
}
#endif

#endif /* CXGN_MACROS_H */
