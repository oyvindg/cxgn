#ifndef CXGN_BATCH_H
#define CXGN_BATCH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cxgn_generator cxgn_generator;
typedef struct cxgn_output cxgn_output;
typedef struct cxgn_error cxgn_error;

typedef struct cxgn_batch cxgn_batch;

/**
 * @brief Options controlling map key derivation and output symbol names.
 */
typedef struct {
    /**
     * Directory prefix stripped from each file path when deriving the map key.
     * NULL: use filename stem only (collision -> error).
     */
    const char* map_root;

    /**
     * C identifier for the emitted map array variable.
     * NULL: defaults to "cxgn_config_map".
     */
    const char* map_name;

    /**
     * C identifier for the map entry struct typedef.
     * NULL: defaults to "cxgn_map_entry_t".
     */
    const char* map_type;

    /**
     * Continue generating other entries when one YAML file fails.
     * false: abort on first failure.
     */
    bool continue_on_error;
} cxgn_batch_options;

/**
 * @brief Per-file batch generation result.
 */
typedef struct {
    char* yaml_path;      /**< Owned source path */
    char* key;            /**< Owned derived map key, or NULL on failure before key derivation */
    char* identifier;     /**< Owned C identifier derived from key, or NULL */
    cxgn_output* output;  /**< Generated per-entry output, or NULL on failure */
    cxgn_error error;     /**< Per-entry error when output is NULL */
} cxgn_batch_entry_result;

/**
 * @brief Aggregated batch generation result.
 */
typedef struct {
    cxgn_output* combined_output;       /**< Combined output, or NULL if nothing succeeded */
    cxgn_batch_entry_result* entries;   /**< Owned per-entry results, sorted by path */
    size_t entry_count;
    size_t success_count;
    size_t failure_count;
} cxgn_batch_result;

/**
 * @brief Initialize batch options with cxgn defaults.
 * @param options Output options struct
 */
void cxgn_batch_options_init(cxgn_batch_options* options);

/**
 * @brief Release all memory owned by a batch result.
 * @param result Result to clear (NULL-safe)
 */
void cxgn_batch_result_clear(cxgn_batch_result* result);

/**
 * @brief Create a new batch.
 * @param gen Generator instance (retained internally)
 * @return Batch handle, or NULL on allocation failure
 */
cxgn_batch* cxgn_batch_new(cxgn_generator* gen);

/**
 * @brief Free a batch handle.
 * @param batch Batch to release (NULL-safe)
 */
void cxgn_batch_free(cxgn_batch* batch);

/**
 * @brief Retain a batch for shared ownership.
 * @param batch Batch handle
 * @return The same pointer for chaining
 */
cxgn_batch* cxgn_batch_retain(cxgn_batch* batch);

/**
 * @brief Return the number of files currently queued in the batch.
 * @param batch Batch instance
 * @return File count
 */
size_t cxgn_batch_count(const cxgn_batch* batch);

/**
 * @brief Return the YAML path at a given batch index.
 * @param batch Batch instance
 * @param index Zero-based file index
 * @return Borrowed path string, or NULL if out of range
 */
const char* cxgn_batch_get_path(const cxgn_batch* batch, size_t index);

/**
 * @brief Add a single YAML file to the batch.
 * @param batch Batch instance
 * @param yaml_path Path to a YAML file (must exist)
 * @param err Error output (can be NULL)
 * @return true on success
 */
bool cxgn_batch_add_file(cxgn_batch* batch, const char* yaml_path, cxgn_error* err);

/**
 * @brief Add all YAML files matching a glob pattern to the batch.
 *
 * Supports *, ?, [...], and ** (recursive directory descent).
 * Files are added in lexicographic order. Duplicates are silently ignored.
 * Zero matches is not an error.
 *
 * @param batch Batch instance
 * @param pattern Glob pattern
 * @param err Error output (can be NULL)
 * @return true on success
 */
bool cxgn_batch_add_glob(cxgn_batch* batch, const char* pattern, cxgn_error* err);

/**
 * @brief Generate a combined output from all queued files.
 *
 * Each YAML file is generated with a unique symbol prefix to avoid name
 * collisions. A keyed map array is appended at the end of the output.
 * The same schema header is used for all entries.
 *
 * @param batch       Batch instance (must contain at least one file)
 * @param header_path Path to the C schema header
 * @param options     Key derivation and naming options (NULL = defaults)
 * @param err         Error output (can be NULL)
 * @return Output handle on success, NULL on error
 */
cxgn_output* cxgn_batch_generate(cxgn_batch* batch,
                                 const char* header_path,
                                 const cxgn_batch_options* options,
                                 cxgn_error* err);

/**
 * @brief Generate a combined output and collect per-entry results.
 *
 * The result is reset by the callee before use. When
 * options->continue_on_error is true, generation continues after per-file
 * failures and successful entries are still emitted into combined_output.
 *
 * @param batch       Batch instance (must contain at least one file)
 * @param header_path Path to the C schema header
 * @param options     Key derivation and error-handling options (NULL = defaults)
 * @param result      Output result struct (must not be NULL)
 * @param err         Batch-level error output (can be NULL)
 * @return true when at least one entry generated successfully
 */
bool cxgn_batch_generate_detailed(cxgn_batch* batch,
                                  const char* header_path,
                                  const cxgn_batch_options* options,
                                  cxgn_batch_result* result,
                                  cxgn_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CXGN_BATCH_H */
