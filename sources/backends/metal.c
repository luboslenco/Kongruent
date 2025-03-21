#include "metal.h"

#include "../analyzer.h"
#include "../compiler.h"
#include "../errors.h"
#include "../functions.h"
#include "../parser.h"
#include "../shader_stage.h"
#include "../types.h"
#include "cstyle.h"
#include "util.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *type_string(type_id type) {
	if (type == float_id) {
		return "float";
	}
	if (type == float2_id) {
		return "float2";
	}
	if (type == float3_id) {
		return "float3";
	}
	if (type == float4_id) {
		return "float4";
	}
	if (type == float4x4_id) {
		return "float4x4";
	}
	return get_name(get_type(type)->name);
}

static char *function_string(name_id func) {
	return get_name(func);
}

static void write_code(char *metal, char *directory, const char *filename) {
	char full_filename[512];
	sprintf(full_filename, "%s/%s.metal", directory, filename);

	FILE *file = fopen(full_filename, "wb");
	fprintf(file, "%s", metal);
	fclose(file);
}

static type_id vertex_inputs[256];
static size_t  vertex_inputs_size = 0;
static type_id fragment_inputs[256];
static size_t  fragment_inputs_size = 0;

static bool is_vertex_input(type_id t) {
	for (size_t i = 0; i < vertex_inputs_size; ++i) {
		if (t == vertex_inputs[i]) {
			return true;
		}
	}
	return false;
}

static bool is_fragment_input(type_id t) {
	for (size_t i = 0; i < fragment_inputs_size; ++i) {
		if (t == fragment_inputs[i]) {
			return true;
		}
	}
	return false;
}

static void type_name(type_id id, char *output_name) {
	type *t = get_type(id);

	if (t->name == NO_NAME) {
		bool found = false;
		for (global_id j = 0; get_global(j)->type != NO_TYPE; ++j) {
			global *g = get_global(j);
			if (g->type == id) {
				sprintf(output_name, "_%" PRIu64 "_type", g->var_index);
				found = true;
				break;
			}
		}

		if (!found) {
			strcpy(output_name, "Unknown");
		}
	}
	else {
		strcpy(output_name, get_name(t->name));
	}
}

static void write_types(char *metal, size_t *offset) {
	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);

		if (!t->built_in && !has_attribute(&t->attributes, add_name("pipe"))) {
			char name[256];
			type_name(i, name);
			*offset += sprintf(&metal[*offset], "struct %s {\n", name);

			if (is_vertex_input(i)) {
				for (size_t j = 0; j < t->members.size; ++j) {
					*offset +=
					    sprintf(&metal[*offset], "\t%s %s [[attribute(%zu)]];\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name), j);
				}
			}
			else if (is_fragment_input(i)) {
				for (size_t j = 0; j < t->members.size; ++j) {
					if (j == 0) {
						*offset += sprintf(&metal[*offset], "\t%s %s [[position]];\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
					}
					else {
						*offset += sprintf(&metal[*offset], "\t%s %s [[user(locn%zu)]];\n", type_string(t->members.m[j].type.type),
						                   get_name(t->members.m[j].name), j - 1);
					}
				}
			}
			else {
				for (size_t j = 0; j < t->members.size; ++j) {
					*offset += sprintf(&metal[*offset], "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
				}
			}
			*offset += sprintf(&metal[*offset], "};\n\n");
		}
	}
}

static int global_register_indices[512];

static function_id vertex_functions[256];
static size_t      vertex_functions_size = 0;
static function_id fragment_functions[256];
static size_t      fragment_functions_size = 0;

static bool is_vertex_function(function_id f) {
	for (size_t i = 0; i < vertex_functions_size; ++i) {
		if (f == vertex_functions[i]) {
			return true;
		}
	}
	return false;
}

static bool is_fragment_function(function_id f) {
	for (size_t i = 0; i < fragment_functions_size; ++i) {
		if (f == fragment_functions[i]) {
			return true;
		}
	}
	return false;
}

static void write_argument_buffers(char *code, size_t *offset) {
	for (size_t set_index = 0; set_index < get_sets_count(); ++set_index) {
		descriptor_set *set = get_set(set_index);

		if (set->name == add_name("root_constants")) {
			if (set->globals.size != 1) {
				debug_context context = {0};
				error(context, "More than one root constants struct found");
			}

			uint32_t  size = 0;
			global_id g    = set->globals.globals[0];

			if (get_type(get_global(g)->type)->built_in) {
				debug_context context = {0};
				error(context, "Unsupported type for a root constant");
			}

			size += struct_size(get_global(g)->type);

			*offset += sprintf(&code[*offset], "\\\n, RootConstants(num32BitConstants=%i)", size / 4);

			continue;
		}

		*offset += sprintf(&code[*offset], "struct %s {\n", get_name(set->name));

		for (size_t global_index = 0; global_index < set->globals.size; ++global_index) {
			global_id g_id     = set->globals.globals[global_index];
			global   *g        = get_global(g_id);
			bool      writable = set->globals.writable[global_index];

			if (!get_type(g->type)->built_in) {
				if (!has_attribute(&g->attributes, add_name("indexed"))) {
					char name[256];
					type_name(g->type, name);
					*offset += sprintf(&code[*offset], "\tdevice %s *_%" PRIu64 " [[id(%zu)]];\n", name, g->var_index,
					                   global_index); // TODO: Handle constant data and set it via constantDataAtIndex
				}
			}
			else if (is_texture(g->type)) {
				if (writable) {
					*offset += sprintf(&code[*offset], "\ttexture2d<float, access::write> %s [[id(%zu)]];\n", get_name(g->name), global_index);
				}
				else {
					*offset += sprintf(&code[*offset], "\ttexture2d<float> %s [[id(%zu)]];\n", get_name(g->name), global_index);
				}
			}
			else if (is_sampler(g->type)) {
				*offset += sprintf(&code[*offset], "\tsampler %s [[id(%zu)]];\n", get_name(g->name), global_index);
			}
			else {
				type *t = get_type(g->type);
				if (t->array_size > 0) {
					*offset += sprintf(&code[*offset], "\tdevice %s *%s [[id(%zu)]];\n", get_name(get_type(g->type)->name), get_name(g->name), global_index);
				}
			}
		}

		*offset += sprintf(&code[*offset], "};\n\n");
	}
}

static void write_functions(char *code, size_t *offset) {
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		if (f->block == NULL) {
			continue;
		}

		uint8_t *data = f->code.o;
		size_t   size = f->code.size;

		uint64_t parameter_ids[256] = {0};
		for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
			for (size_t i = 0; i < f->block->block.vars.size; ++i) {
				if (f->parameter_names[parameter_index] == f->block->block.vars.v[i].name) {
					parameter_ids[parameter_index] = f->block->block.vars.v[i].variable_id;
					break;
				}
			}
		}

		debug_context context = {0};
		for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
			check(parameter_ids[parameter_index] != 0, context, "Parameter not found");
		}

		char buffers[1024];
		strcpy(buffers, "");
		if (is_vertex_function(i) || is_fragment_function(i)) {
			global_array globals = {0};
			find_referenced_globals(f, &globals);

			size_t buffers_offset = 0;

			descriptor_set_group *set_group = get_descriptor_set_group(f->descriptor_set_group_index);

			for (size_t set_index = 0; set_index < set_group->size; ++set_index) {
				descriptor_set *set = set_group->values[set_index];

				buffers_offset +=
				    sprintf(&buffers[buffers_offset], ", constant %s& argument_buffer%zu [[buffer(%zu)]]", get_name(set->name), set_index, set_index);
			}
		}

		if (is_vertex_function(i)) {
			*offset += sprintf(&code[*offset], "vertex %s %s(%s _%" PRIu64 " [[stage_in]]", type_string(f->return_type.type), get_name(f->name),
			                   type_string(f->parameter_types[0].type), parameter_ids[0]);
			for (uint8_t parameter_index = 1; parameter_index < f->parameters_size; ++parameter_index) {
				*offset += sprintf(&code[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[0].type), parameter_ids[0]);
			}
			*offset += sprintf(&code[*offset], "%s) {\n", buffers);
		}
		else if (is_fragment_function(i)) {
			if (get_type(f->return_type.type)->array_size > 0) {
				*offset += sprintf(&code[*offset], "struct _kong_colors_out {\n");
				for (uint32_t j = 0; j < get_type(f->return_type.type)->array_size; ++j) {
					*offset += sprintf(&code[*offset], "\t%s _%i [[color(%i)]];\n", type_string(f->return_type.type), j, j);
				}
				*offset += sprintf(&code[*offset], "};\n\n");

				*offset += sprintf(&code[*offset], "fragment _kong_colors_out %s(%s _%" PRIu64 " [[stage_in]]", get_name(f->name),
				                   type_string(f->parameter_types[0].type), parameter_ids[0]);
				for (uint8_t parameter_index = 1; parameter_index < f->parameters_size; ++parameter_index) {
					*offset += sprintf(&code[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				*offset += sprintf(&code[*offset], "%s) {\n", buffers);
			}
			else {
				*offset += sprintf(&code[*offset], "fragment _kong_color_out %s(%s _%" PRIu64 " [[stage_in]]", get_name(f->name),
				                   type_string(f->parameter_types[0].type), parameter_ids[0]);
				for (uint8_t parameter_index = 1; parameter_index < f->parameters_size; ++parameter_index) {
					*offset += sprintf(&code[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				*offset += sprintf(&code[*offset], "%s) {\n", buffers);
			}
		}
		else {
			*offset += sprintf(&code[*offset], "%s %s(", type_string(f->return_type.type), get_name(f->name));
			for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
				if (parameter_index == 0) {
					*offset += sprintf(&code[*offset], "%s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
				else {
					*offset += sprintf(&code[*offset], ", %s _%" PRIu64, type_string(f->parameter_types[parameter_index].type), parameter_ids[parameter_index]);
				}
			}
			*offset += sprintf(&code[*offset], ") {\n");
		}

		int indentation = 1;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];
			switch (o->type) {
			case OPCODE_LOAD_MEMBER: {
				uint64_t global_var_index = 0;
				for (global_id j = 0; get_global(j) != NULL && get_global(j)->type != NO_TYPE; ++j) {
					global *g = get_global(j);
					if (o->op_load_member.from.index == g->var_index) {
						global_var_index = g->var_index;
						break;
					}
				}

				indent(code, offset, indentation);
				if (global_var_index != 0) {
					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = argument_buffer0._%" PRIu64, type_string(o->op_load_member.to.type.type),
					                   o->op_load_member.to.index, o->op_load_member.from.index);
				}
				else {
					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type.type), o->op_load_member.to.index,
					                   o->op_load_member.from.index);
				}
				type *s = get_type(o->op_load_member.member_parent_type);

				for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
					if (i == 0 && global_var_index != 0) {
						*offset += sprintf(&code[*offset], "->%s", get_name(s->members.m[o->op_load_member.static_member_indices[i]].name));
					}
					else {
						*offset += sprintf(&code[*offset], ".%s", get_name(s->members.m[o->op_load_member.static_member_indices[i]].name));
					}
					s = get_type(s->members.m[o->op_load_member.static_member_indices[i]].type.type);
				}

				*offset += sprintf(&code[*offset], ";\n");

				break;
			}
			case OPCODE_RETURN: {
				if (o->size > offsetof(opcode, op_return)) {
					if (is_fragment_function(i) && get_type(f->return_type.type)->array_size > 0) {
						indent(code, offset, indentation);
						*offset += sprintf(&code[*offset], "{\n");
						indent(code, offset, indentation + 1);
						*offset += sprintf(&code[*offset], "_kong_colors_out _kong_colors;\n");
						for (uint32_t j = 0; j < get_type(f->return_type.type)->array_size; ++j) {
							indent(code, offset, indentation + 1);
							*offset += sprintf(&code[*offset], "_kong_colors._%i = _%" PRIu64 "[%i];\n", j, o->op_return.var.index, j);
						}
						indent(code, offset, indentation + 1);
						*offset += sprintf(&code[*offset], "return _kong_colors;\n");
						indent(code, offset, indentation);
						*offset += sprintf(&code[*offset], "}\n");
					}
					else if (is_fragment_function(i)) {
						indent(code, offset, indentation);
						*offset += sprintf(&code[*offset], "{\n");
						indent(code, offset, indentation + 1);
						*offset += sprintf(&code[*offset], "_kong_color_out _kong_color;\n");
						indent(code, offset, indentation + 1);
						*offset += sprintf(&code[*offset], "_kong_color._0 = _%" PRIu64 ";\n", o->op_return.var.index);
						indent(code, offset, indentation + 1);
						*offset += sprintf(&code[*offset], "return _kong_color;\n");
						indent(code, offset, indentation);
						*offset += sprintf(&code[*offset], "}\n");
					}
					else {
						indent(code, offset, indentation);
						*offset += sprintf(&code[*offset], "return _%" PRIu64 ";\n", o->op_return.var.index);
					}
				}
				else {
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "return;\n");
				}
				break;
			}
			case OPCODE_CALL: {
				debug_context context = {0};
				if (o->op_call.func == add_name("sample")) {
					check(o->op_call.parameters_size == 3, context, "sample requires three parameters");
					indent(code, offset, indentation);
					*offset +=
					    sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 ".sample(_%" PRIu64 ", _%" PRIu64 ");\n", type_string(o->op_call.var.type.type),
					            o->op_call.var.index, o->op_call.parameters[0].index, o->op_call.parameters[1].index, o->op_call.parameters[2].index);
				}
				else if (o->op_call.func == add_name("sample_lod")) {
					check(o->op_call.parameters_size == 4, context, "sample_lod requires four parameters");
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 ".sample(_%" PRIu64 ", _%" PRIu64 ", level(_%" PRIu64 "));\n",
					                   type_string(o->op_call.var.type.type), o->op_call.var.index, o->op_call.parameters[0].index,
					                   o->op_call.parameters[1].index, o->op_call.parameters[2].index, o->op_call.parameters[3].index);
				}
				else {
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = %s(", type_string(o->op_call.var.type.type), o->op_call.var.index,
					                   function_string(o->op_call.func));
					if (o->op_call.parameters_size > 0) {
						*offset += sprintf(&code[*offset], "_%" PRIu64, o->op_call.parameters[0].index);
						for (uint8_t i = 1; i < o->op_call.parameters_size; ++i) {
							*offset += sprintf(&code[*offset], ", _%" PRIu64, o->op_call.parameters[i].index);
						}
					}
					*offset += sprintf(&code[*offset], ");\n");
				}
				break;
			}
			default:
				cstyle_write_opcode(code, offset, o, type_string, &indentation);
				break;
			}

			index += o->size;
		}

		*offset += sprintf(&code[*offset], "}\n\n");
	}
}

static void metal_export_everything(char *directory) {
	char         *metal   = (char *)calloc(1024 * 1024, 1);
	debug_context context = {0};
	check(metal != NULL, context, "Could not allocate Metal string");
	size_t offset = 0;

	offset += sprintf(&metal[offset], "#include <metal_stdlib>\n");
	offset += sprintf(&metal[offset], "#include <simd/simd.h>\n\n");
	offset += sprintf(&metal[offset], "using namespace metal;\n\n");

	offset += sprintf(&metal[offset], "struct _kong_color_out {\n");
	offset += sprintf(&metal[offset], "\tfloat4 _0 [[color(0)]];\n");
	offset += sprintf(&metal[offset], "};\n\n");

	write_types(metal, &offset);

	write_argument_buffers(metal, &offset);

	write_functions(metal, &offset);

	write_code(metal, directory, "kong");
}

void metal_export(char *directory) {
	int cbuffer_index = 0;
	int texture_index = 0;
	int sampler_index = 0;

	memset(global_register_indices, 0, sizeof(global_register_indices));

	for (global_id i = 0; get_global(i) != NULL && get_global(i)->type != NO_TYPE; ++i) {
		global *g = get_global(i);
		if (g->type == sampler_type_id) {
			global_register_indices[i] = sampler_index;
			sampler_index += 1;
		}
		else if (g->type == tex2d_type_id || g->type == texcube_type_id) {
			global_register_indices[i] = texture_index;
			texture_index += 1;
		}
		else if (g->type == float_id) {
		}
		else {
			global_register_indices[i] = cbuffer_index;
			cbuffer_index += 1;
		}
	}

	for (type_id i = 0; get_type(i) != NULL; ++i) {
		type *t = get_type(i);
		if (!t->built_in && has_attribute(&t->attributes, add_name("pipe"))) {
			name_id vertex_shader_name   = NO_NAME;
			name_id fragment_shader_name = NO_NAME;

			for (size_t j = 0; j < t->members.size; ++j) {
				if (t->members.m[j].name == add_name("vertex")) {
					vertex_shader_name = t->members.m[j].value.identifier;
				}
				else if (t->members.m[j].name == add_name("fragment")) {
					fragment_shader_name = t->members.m[j].value.identifier;
				}
			}

			debug_context context = {0};
			check(vertex_shader_name != NO_NAME, context, "vertex shader missing");
			check(fragment_shader_name != NO_NAME, context, "fragment shader missing");

			for (function_id i = 0; get_function(i) != NULL; ++i) {
				function *f = get_function(i);
				if (f->name == vertex_shader_name) {
					vertex_functions[vertex_functions_size] = i;
					vertex_functions_size += 1;

					assert(f->parameters_size > 0);
					vertex_inputs[vertex_inputs_size] = f->parameter_types[0].type;
					vertex_inputs_size += 1;
				}
				else if (f->name == fragment_shader_name) {
					fragment_functions[fragment_functions_size] = i;
					fragment_functions_size += 1;

					assert(f->parameters_size > 0);
					fragment_inputs[fragment_inputs_size] = f->parameter_types[0].type;
					fragment_inputs_size += 1;
				}
			}
		}
	}

	metal_export_everything(directory);
}
