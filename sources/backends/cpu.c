#include "glsl.h"

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
		return "kore_float2";
	}
	if (type == float3_id) {
		return "kore_float3";
	}
	if (type == float4_id) {
		return "kore_float4";
	}
	if (type == float4x4_id) {
		return "kore_matrix4x4";
	}
	if (type == int2_id) {
		return "kore_int2";
	}
	if (type == int3_id) {
		return "kore_int3";
	}
	if (type == int4_id) {
		return "kore_int4";
	}
	if (type == uint_id) {
		return "uint32_t";
	}
	if (type == uint2_id) {
		return "kore_uint2";
	}
	if (type == uint3_id) {
		return "kore_uint3";
	}
	if (type == uint4_id) {
		return "kore_uint4";
	}
	return get_name(get_type(type)->name);
}

static void write_code(char *code, char *directory, const char *filename, const char *name) {
	char full_filename[512];

	{
		sprintf(full_filename, "%s/%s.h", directory, filename);
		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include <stddef.h>\n\n");
		fprintf(file, "void %s(uint32_t workgroup_count_x, uint32_t workgroup_count_y, uint32_t workgroup_count_z);\n", name);
		fclose(file);
	}

	{
		sprintf(full_filename, "%s/%s.c", directory, filename);

		FILE *file = fopen(full_filename, "wb");
		fprintf(file, "#include \"%s.h\"\n\n", filename);

		fprintf(file, "#include <kore3/math/vector.h>\n");
		fprintf(file, "#include <kore3/util/cpucompute.h>\n\n");

		fprintf(file, "%s", code);

		fclose(file);
	}
}

static void write_types(char *code, size_t *offset, shader_stage stage, type_id input, type_id output, function *main) {
	type_id types[256];
	size_t types_size = 0;
	find_referenced_types(main, types, &types_size);

	for (size_t i = 0; i < types_size; ++i) {
		type *t = get_type(types[i]);

		if (!t->built_in && !has_attribute(&t->attributes, add_name("pipe"))) {
			*offset += sprintf(&code[*offset], "struct %s {\n", get_name(t->name));

			for (size_t j = 0; j < t->members.size; ++j) {
				*offset += sprintf(&code[*offset], "\t%s %s;\n", type_string(t->members.m[j].type.type), get_name(t->members.m[j].name));
			}

			*offset += sprintf(&code[*offset], "};\n\n");
		}
	}
}

static void write_globals(char *code, size_t *offset, function *main) {
	global_id globals[256];
	size_t globals_size = 0;
	find_referenced_globals(main, globals, &globals_size);

	for (size_t i = 0; i < globals_size; ++i) {
		global *g = get_global(globals[i]);

		if (g->type == sampler_type_id) {
		}
		else if (g->type == tex2d_type_id) {
		}
		else if (g->type == texcube_type_id) {
		}
		else if (g->type == float_id) {
		}
		else {
			*offset += sprintf(&code[*offset], "typedef struct _%" PRIu64 " {\n", g->var_index);
			type *t = get_type(g->type);
			for (size_t i = 0; i < t->members.size; ++i) {
				*offset += sprintf(&code[*offset], "\t%s %s;\n", type_string(t->members.m[i].type.type), get_name(t->members.m[i].name));
			}
			*offset += sprintf(&code[*offset], "} _%" PRIu64 ";\n\n", g->var_index);
		}
	}
}

static void write_functions(char *code, const char *name, size_t *offset, shader_stage stage, type_id input, type_id output, function *main, bool flip) {
	function *functions[256];
	size_t functions_size = 0;

	functions[functions_size] = main;
	functions_size += 1;

	find_referenced_functions(main, functions, &functions_size);

	for (size_t i = 0; i < functions_size; ++i) {
		function *f = functions[i];

		debug_context context = {0};
		check(f->block != NULL, context, "Function has no block");

		uint8_t *data = f->code.o;
		size_t size = f->code.size;

		uint64_t parameter_ids[256] = {0};
		for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
			for (size_t i = 0; i < f->block->block.vars.size; ++i) {
				if (f->parameter_names[parameter_index] == f->block->block.vars.v[i].name) {
					parameter_ids[parameter_index] = f->block->block.vars.v[i].variable_id;
					break;
				}
			}
		}

		for (uint8_t parameter_index = 0; parameter_index < f->parameters_size; ++parameter_index) {
			check(parameter_ids[parameter_index] != 0, context, "Parameter not found");
		}

		if (f == main) {
			if (stage == SHADER_STAGE_COMPUTE) {
				attribute *threads_attribute = find_attribute(&f->attributes, add_name("threads"));
				if (threads_attribute == NULL || threads_attribute->paramters_count != 3) {
					debug_context context = {0};
					error(context, "Compute function requires a threads attribute with three parameters");
				}

				*offset += sprintf(&code[*offset], "void %s(uint32_t workgroup_count_x, uint32_t workgroup_count_y, uint32_t workgroup_count_z) {\n", name);

				*offset += sprintf(&code[*offset], "\tuint32_t local_size_x = %i; uint32_t local_size_y = %i; uint32_t local_size_z = %i;\n",
				                   (int)threads_attribute->parameters[0], (int)threads_attribute->parameters[1], (int)threads_attribute->parameters[2]);

				*offset += sprintf(&code[*offset],
				                   "\tuint32_t group_id = 0; uint32_t group_thread_id = 0; uint32_t dispatch_thread_id = 0; uint32_t group_index = 0;\n\n");
			}
			else {
				debug_context context = {0};
				error(context, "Unsupported shader stage");
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
			case OPCODE_CALL: {
				if (o->op_call.func == add_name("group_id")) {
					check(o->op_call.parameters_size == 0, context, "group_id can not have a parameter");
					*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = group_id;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("group_thread_id")) {
					check(o->op_call.parameters_size == 0, context, "group_thread_id can not have a parameter");
					*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = group_thread_id;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("dispatch_thread_id")) {
					check(o->op_call.parameters_size == 0, context, "dispatch_thread_id can not have a parameter");
					*offset +=
					    sprintf(&code[*offset], "\t%s _%" PRIu64 " = dispatch_thread_id;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else if (o->op_call.func == add_name("group_index")) {
					check(o->op_call.parameters_size == 0, context, "group_index can not have a parameter");
					*offset += sprintf(&code[*offset], "\t%s _%" PRIu64 " = group_index;\n", type_string(o->op_call.var.type.type), o->op_call.var.index);
				}
				else {
					const char *function_name = get_name(o->op_call.func);
					if (o->op_call.func == add_name("float")) {
						function_name = "create_float";
					}
					else if (o->op_call.func == add_name("float2")) {
						function_name = "create_float2";
					}
					else if (o->op_call.func == add_name("float3")) {
						function_name = "create_float3";
					}
					else if (o->op_call.func == add_name("float4")) {
						function_name = "create_float4";
					}

					indent(code, offset, indentation);

					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = kore_cpu_compute_%s", type_string(o->op_call.var.type.type), o->op_call.var.index,
					                   function_name);

					for (uint8_t parameter_index = 0; parameter_index < o->op_call.parameters_size; ++parameter_index) {
						variable v = o->op_call.parameters[parameter_index];
						if (v.type.type == int_id) {
							*offset += sprintf(&code[*offset], "_i1");
						}
						else if (v.type.type == int2_id) {
							*offset += sprintf(&code[*offset], "_i2");
						}
						else if (v.type.type == int3_id) {
							*offset += sprintf(&code[*offset], "_i3");
						}
						else if (v.type.type == int4_id) {
							*offset += sprintf(&code[*offset], "_i4");
						}
						else if (v.type.type == uint_id) {
							*offset += sprintf(&code[*offset], "_u1");
						}
						else if (v.type.type == uint2_id) {
							*offset += sprintf(&code[*offset], "_u2");
						}
						else if (v.type.type == uint3_id) {
							*offset += sprintf(&code[*offset], "_u3");
						}
						else if (v.type.type == uint4_id) {
							*offset += sprintf(&code[*offset], "_u4");
						}
						else if (v.type.type == float_id) {
							*offset += sprintf(&code[*offset], "_f1");
						}
						else if (v.type.type == float2_id) {
							*offset += sprintf(&code[*offset], "_f2");
						}
						else if (v.type.type == float3_id) {
							*offset += sprintf(&code[*offset], "_f3");
						}
						else if (v.type.type == float4_id) {
							*offset += sprintf(&code[*offset], "_f4");
						}
						else {
							debug_context context = {0};
							error(context, "Unknown parameter type");
						}
					}

					*offset += sprintf(&code[*offset], "(");

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
				if (f == main && o->op_load_member.member_parent_type == input) {
					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = %s", type_string(o->op_load_member.to.type.type), o->op_load_member.to.index,
					                   type_string(o->op_load_member.member_parent_type));
				}
				else {
					*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64, type_string(o->op_load_member.to.type.type), o->op_load_member.to.index,
					                   o->op_load_member.from.index);
				}

				type *s = get_type(o->op_load_member.member_parent_type);
				for (size_t i = 0; i < o->op_load_member.member_indices_size; ++i) {
					if (f == main && o->op_load_member.member_parent_type == input && i == 0) {
						*offset += sprintf(&code[*offset], ".%s", get_name(s->members.m[o->op_load_member.static_member_indices[i]].name));
					}
					else if (global_var_index != 0) {
						*offset += sprintf(&code[*offset], ".%s", get_name(s->members.m[o->op_load_member.static_member_indices[i]].name));
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
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "return _%" PRIu64 ";\n", o->op_return.var.index);
				}
				else {
					indent(code, offset, indentation);
					*offset += sprintf(&code[*offset], "return;\n");
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

static void cpu_export_compute(char *directory, function *main) {
	char *code = (char *)calloc(1024 * 1024, 1);
	debug_context context = {0};
	check(code != NULL, context, "Could not allocate code string");

	size_t offset = 0;

	assert(main->parameters_size == 0);

	write_types(code, &offset, SHADER_STAGE_COMPUTE, NO_TYPE, NO_TYPE, main);

	write_globals(code, &offset, main);

	char *name = get_name(main->name);

	char func_name[256];
	sprintf(func_name, "%s_on_cpu", name);

	write_functions(code, func_name, &offset, SHADER_STAGE_COMPUTE, NO_TYPE, NO_TYPE, main, false);

	char filename[512];
	sprintf(filename, "kong_cpu_%s", name);

	write_code(code, directory, filename, func_name);
}

void cpu_export(char *directory) {
	function *compute_shaders[256];
	size_t compute_shaders_size = 0;

	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);
		if (has_attribute(&f->attributes, add_name("compute"))) {
			compute_shaders[compute_shaders_size] = f;
			compute_shaders_size += 1;
		}
	}

	for (size_t i = 0; i < compute_shaders_size; ++i) {
		cpu_export_compute(directory, compute_shaders[i]);
	}
}
