#pragma once

#include "../shader_stage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int compile_hlsl_to_d3d9(const char *source, uint8_t **output, size_t *outputlength, shader_stage stage, bool debug);
