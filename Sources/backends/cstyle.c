#include "cstyle.h"

#include "../errors.h"
#include "util.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static char *function_string(name_id func) {
	return get_name(func);
}

void cstyle_write_opcode(char *code, size_t *offset, opcode *o, type_string_func type_string, int *indentation) {
	switch (o->type) {
	case OPCODE_VAR:
		indent(code, offset, *indentation);
		if (o->op_var.var.type.array_size > 0) {
			*offset +=
			    sprintf(&code[*offset], "%s _%" PRIu64 "[%i];\n", type_string(o->op_var.var.type.type), o->op_var.var.index, o->op_var.var.type.array_size);
		}
		else {
			*offset += sprintf(&code[*offset], "%s _%" PRIu64 ";\n", type_string(o->op_var.var.type.type), o->op_var.var.index);
		}
		break;
	case OPCODE_NOT:
		indent(code, offset, *indentation);
		*offset +=
		    sprintf(&code[*offset], "%s _%" PRIu64 " = !_%" PRIu64 ";\n", type_string(o->op_not.to.type.type), o->op_not.to.index, o->op_not.from.index);
		break;
	case OPCODE_STORE_VARIABLE:
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "_%" PRIu64 " = _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
		break;
	case OPCODE_SUB_AND_STORE_VARIABLE:
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "_%" PRIu64 " -= _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
		break;
	case OPCODE_ADD_AND_STORE_VARIABLE:
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "_%" PRIu64 " += _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
		break;
	case OPCODE_DIVIDE_AND_STORE_VARIABLE:
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "_%" PRIu64 " /= _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
		break;
	case OPCODE_MULTIPLY_AND_STORE_VARIABLE:
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "_%" PRIu64 " *= _%" PRIu64 ";\n", o->op_store_var.to.index, o->op_store_var.from.index);
		break;
	case OPCODE_STORE_MEMBER:
	case OPCODE_SUB_AND_STORE_MEMBER:
	case OPCODE_ADD_AND_STORE_MEMBER:
	case OPCODE_DIVIDE_AND_STORE_MEMBER:
	case OPCODE_MULTIPLY_AND_STORE_MEMBER:
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "_%" PRIu64, o->op_store_member.to.index);
		type *s = get_type(o->op_store_member.member_parent_type);
		bool is_array = o->op_store_member.member_parent_array;
		for (size_t i = 0; i < o->op_store_member.member_indices_size; ++i) {
			if (is_array) {
				*offset += sprintf(&code[*offset], "[%i]", o->op_store_member.member_indices[i]);
				is_array = false;
			}
			else {
				debug_context context = {0};
				check(o->op_store_member.member_indices[i] < s->members.size, context, "Member index out of bounds");
				*offset += sprintf(&code[*offset], ".%s", get_name(s->members.m[o->op_store_member.member_indices[i]].name));
				is_array = s->members.m[o->op_store_member.member_indices[i]].type.array_size > 0;
				s = get_type(s->members.m[o->op_store_member.member_indices[i]].type.type);
			}
		}
		switch (o->type) {
		case OPCODE_STORE_MEMBER:
			*offset += sprintf(&code[*offset], " = _%" PRIu64 ";\n", o->op_store_member.from.index);
			break;
		case OPCODE_SUB_AND_STORE_MEMBER:
			*offset += sprintf(&code[*offset], " -= _%" PRIu64 ";\n", o->op_store_member.from.index);
			break;
		case OPCODE_ADD_AND_STORE_MEMBER:
			*offset += sprintf(&code[*offset], " += _%" PRIu64 ";\n", o->op_store_member.from.index);
			break;
		case OPCODE_DIVIDE_AND_STORE_MEMBER:
			*offset += sprintf(&code[*offset], " /= _%" PRIu64 ";\n", o->op_store_member.from.index);
			break;
		case OPCODE_MULTIPLY_AND_STORE_MEMBER:
			*offset += sprintf(&code[*offset], " *= _%" PRIu64 ";\n", o->op_store_member.from.index);
			break;
		}
		break;
	case OPCODE_LOAD_CONSTANT:
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = %f;\n", type_string(o->op_load_constant.to.type.type), o->op_load_constant.to.index,
		                   o->op_load_constant.number);
		break;
	case OPCODE_CALL: {
		indent(code, offset, *indentation);
		debug_context context = {0};
		if (o->op_call.func == add_name("sample")) {
			check(o->op_call.parameters_size == 3, context, "sample requires three parameters");
			*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 ".Sample(_%" PRIu64 ", _%" PRIu64 ");\n", type_string(o->op_call.var.type.type),
			                   o->op_call.var.index, o->op_call.parameters[0].index, o->op_call.parameters[1].index, o->op_call.parameters[2].index);
		}
		else if (o->op_call.func == add_name("sample_lod")) {
			check(o->op_call.parameters_size == 4, context, "sample_lod requires four parameters");
			*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 ".SampleLevel(_%" PRIu64 ", _%" PRIu64 ", _%" PRIu64 ");\n",
			                   type_string(o->op_call.var.type.type), o->op_call.var.index, o->op_call.parameters[0].index, o->op_call.parameters[1].index,
			                   o->op_call.parameters[2].index, o->op_call.parameters[3].index);
		}
		else {
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
	case OPCODE_ADD: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " + _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_SUB: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " - _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_MULTIPLY: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " * _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_DIVIDE: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " / _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_EQUALS: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " == _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_NOT_EQUALS: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " != _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_GREATER: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " > _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_GREATER_EQUAL: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " >= _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_LESS: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " < _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_LESS_EQUAL: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " <= _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_AND: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " && _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_OR: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "%s _%" PRIu64 " = _%" PRIu64 " || _%" PRIu64 ";\n", type_string(o->op_binary.result.type.type),
		                   o->op_binary.result.index, o->op_binary.left.index, o->op_binary.right.index);
		break;
	}
	case OPCODE_IF: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "if (_%" PRIu64 ")\n", o->op_if.condition.index);
		break;
	}
	case OPCODE_WHILE_START: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "while (true)\n");
		*offset += sprintf(&code[*offset], "{\n");
		break;
	}
	case OPCODE_WHILE_CONDITION: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "if (!_%" PRIu64 ") break;\n", o->op_while.condition.index);
		break;
	}
	case OPCODE_WHILE_END: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "}\n");
		break;
	}
	case OPCODE_BLOCK_START: {
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "{\n");
		*indentation += 1;
		break;
	}
	case OPCODE_BLOCK_END: {
		*indentation -= 1;
		indent(code, offset, *indentation);
		*offset += sprintf(&code[*offset], "}\n");
		break;
	}
	default: {
		debug_context context = {0};
		error(context, "Unknown opcode");
		break;
	}
	}
}
