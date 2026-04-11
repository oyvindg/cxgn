/**
 * @file yaml_stubs.c
 * @brief YAML-disabled compatibility stubs for cxgn.
 */

#include "internal.h"

static void set_feature_disabled(cxgn_error* err) {
    cxgn_error_init(err);
    cxgn_error_set(err, CXGN_ERR_FEATURE_DISABLED,
                   "YAML support is not available in this build");
}

void cxgn_validation_options_init(cxgn_validation_options* options) {
    if (!options) return;
    options->strict_mode = false;
    options->unknown_field = CXGN_VALIDATION_WARN;
    options->duplicate_key = CXGN_VALIDATION_ERROR;
    options->missing_field = CXGN_VALIDATION_WARN;
    options->diagnostic_fn = NULL;
    options->diagnostic_userdata = NULL;
}

cxgn_output* cxgn_output_retain(cxgn_output* output) {
    if (output) output->ref_count++;
    return output;
}

const char* cxgn_output_get_code(const cxgn_output* output) {
    return output ? output->code : NULL;
}

size_t cxgn_output_get_code_length(const cxgn_output* output) {
    return output ? output->length : 0;
}

void cxgn_output_free(cxgn_output* output) {
    if (!output) return;
    if (output->ref_count > 1) {
        output->ref_count--;
        return;
    }
    free(output->code);
    free(output);
}

cxgn_generator* cxgn_generator_retain(cxgn_generator* gen) {
    if (gen) gen->ref_count++;
    return gen;
}

cxgn_generator* cxgn_generator_new(const cxgn_struct_parser* parser, const cxgn_string_utils* utils) {
    if (!parser) return NULL;

    cxgn_string_utils* retained_utils =
        cxgn_string_utils_retain((cxgn_string_utils*)(utils ? utils : parser->utils));
    if (!retained_utils) return NULL;

    cxgn_generator* gen = (cxgn_generator*)calloc(1, sizeof(*gen));
    if (!gen) {
        cxgn_string_utils_free(retained_utils);
        return NULL;
    }

    gen->ref_count = 1;
    gen->parser = cxgn_struct_parser_retain((cxgn_struct_parser*)parser);
    gen->utils = retained_utils;
    gen->cpp_std = CXGN_CPP_STD_20;
    cxgn_validation_options_init(&gen->validation);
    return gen;
}

void cxgn_generator_free(cxgn_generator* gen) {
    if (!gen) return;
    if (gen->ref_count > 1) {
        gen->ref_count--;
        return;
    }
    free(gen->helpers_header);
    free(gen->symbol_prefix);
    cxgn_struct_parser_free(gen->parser);
    cxgn_string_utils_free(gen->utils);
    free(gen);
}

void cxgn_generator_set_expression_handler(cxgn_generator* gen,
                                           const cxgn_expression_handler* handler) {
    if (!gen || !handler) return;
    gen->expr_handler = *handler;
    gen->has_expr_handler = true;
}

void cxgn_generator_set_type_options(cxgn_generator* gen, const cxgn_type_options* options) {
    (void)gen;
    (void)options;
}

void cxgn_generator_set_validation_options(cxgn_generator* gen,
                                           const cxgn_validation_options* options) {
    if (!gen || !options) return;
    gen->validation = *options;
}

void cxgn_generator_set_strict_mode(cxgn_generator* gen, bool strict) {
    if (gen) gen->validation.strict_mode = strict;
}

void cxgn_generator_set_cpp_std(cxgn_generator* gen, cxgn_cpp_std std) {
    if (gen) gen->cpp_std = std;
}

void cxgn_generator_set_helpers_header(cxgn_generator* gen, const char* helpers_header) {
    if (!gen) return;
    char* next = helpers_header ? cxgn_strdup(helpers_header) : NULL;
    if (helpers_header && !next) return;
    free(gen->helpers_header);
    gen->helpers_header = next;
}

void cxgn_generator_set_symbol_prefix(cxgn_generator* gen, const char* prefix) {
    if (!gen) return;
    char* next = prefix ? cxgn_strdup(prefix) : NULL;
    if (prefix && !next) return;
    free(gen->symbol_prefix);
    gen->symbol_prefix = next;
}

cxgn_output* cxgn_generate(cxgn_generator* gen, const char* yaml_path,
                           const char* header_path, cxgn_error* err) {
    (void)gen;
    (void)yaml_path;
    (void)header_path;
    set_feature_disabled(err);
    return NULL;
}

cxgn_output* cxgn_generate_from_yaml_text(cxgn_generator* gen, const char* yaml_text,
                                          const char* yaml_virtual_path, const char* header_path,
                                          cxgn_error* err) {
    (void)gen;
    (void)yaml_text;
    (void)yaml_virtual_path;
    (void)header_path;
    set_feature_disabled(err);
    return NULL;
}

cxgn_batch* cxgn_batch_retain(cxgn_batch* batch) {
    if (batch) batch->ref_count++;
    return batch;
}

cxgn_batch* cxgn_batch_new(cxgn_generator* gen) {
    if (!gen) return NULL;
    cxgn_batch* batch = (cxgn_batch*)calloc(1, sizeof(*batch));
    if (!batch) return NULL;
    batch->ref_count = 1;
    batch->gen = cxgn_generator_retain(gen);
    return batch;
}

void cxgn_batch_free(cxgn_batch* batch) {
    if (!batch) return;
    if (batch->ref_count > 1) {
        batch->ref_count--;
        return;
    }
    for (size_t i = 0; i < batch->count; i++) free(batch->yaml_paths[i]);
    free(batch->yaml_paths);
    cxgn_generator_free(batch->gen);
    free(batch);
}

size_t cxgn_batch_count(const cxgn_batch* batch) {
    return batch ? batch->count : 0;
}

const char* cxgn_batch_get_path(const cxgn_batch* batch, size_t index) {
    if (!batch || index >= batch->count) return NULL;
    return batch->yaml_paths[index];
}

bool cxgn_batch_add_file(cxgn_batch* batch, const char* yaml_path, cxgn_error* err) {
    (void)batch;
    (void)yaml_path;
    set_feature_disabled(err);
    return false;
}

bool cxgn_batch_add_glob(cxgn_batch* batch, const char* pattern, cxgn_error* err) {
    (void)batch;
    (void)pattern;
    set_feature_disabled(err);
    return false;
}

cxgn_output* cxgn_batch_generate(cxgn_batch* batch,
                                 const char* header_path,
                                 const cxgn_batch_options* options,
                                 cxgn_error* err) {
    (void)batch;
    (void)header_path;
    (void)options;
    set_feature_disabled(err);
    return NULL;
}
