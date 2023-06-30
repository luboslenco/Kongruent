#include "c.h"

#include "../compiler.h"
#include "../functions.h"
#include "../parser.h"
#include "../types.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

static char *type_string(type_id type) {
	if (type == f32_id) {
		return "float";
	}
	if (type == vec2_id) {
		return "kinc_vector2";
	}
	if (type == vec3_id) {
		return "kinc_vector3";
	}
	if (type == vec4_id) {
		return "kinc_vector4";
	}
	return get_name(get_type(type)->name);
}

void c_export() {
	FILE *output = fopen("test.h", "wb");

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in) {
			fprintf(output, "struct %s {\n", get_name(t->name));
			for (size_t j = 0; j < t->members.size; ++j) {
				fprintf(output, "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
			}
			fprintf(output, "};\n\n");
		}
	}

	fclose(output);
}
