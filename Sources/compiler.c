#include "compiler.h"

#include "errors.h"
#include "parser.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

variable find_local_var(block *b, name_id name) {
	if (b == NULL) {
		variable var;
		var.index = 0;
		var.type = NO_STRUCT;
		return var;
	}

	for (size_t i = 0; i < b->vars.size; ++i) {
		if (b->vars.v[i].name == name) {
			assert(b->vars.v[i].type.resolved);
			variable var;
			var.index = b->vars.v[i].variable_id;
			var.type = b->vars.v[i].type.type;
			return var;
		}
	}

	return find_local_var(b->parent, name);
}

const char all_names[1024 * 1024];

static uint64_t next_variable_id = 1;

opcodes all_opcodes;

variable all_variables[1024 * 1024];

variable allocate_variable(struct_id type) {
	variable v;
	v.index = next_variable_id;
	v.type = type;
	all_variables[v.index] = v;
	++next_variable_id;
	return v;
}

void emit_op(opcode *o) {
	memcpy(&all_opcodes.o[all_opcodes.size], o, o->size);
	all_opcodes.size += o->size;
}

#define OP_SIZE(op, opmember) offsetof(opcode, opmember) + sizeof(o.opmember)

variable emit_expression(block *parent, expression *e) {
	switch (e->kind) {
	case EXPRESSION_BINARY: {
		expression *left = e->binary.left;
		expression *right = e->binary.right;

		switch (e->binary.op) {
		case OPERATOR_EQUALS:
			error("not implemented", 0, 0);
		case OPERATOR_NOT_EQUALS:
			error("not implemented", 0, 0);
		case OPERATOR_GREATER:
			error("not implemented", 0, 0);
		case OPERATOR_GREATER_EQUAL:
			error("not implemented", 0, 0);
		case OPERATOR_LESS:
			error("not implemented", 0, 0);
		case OPERATOR_LESS_EQUAL:
			error("not implemented", 0, 0);
		case OPERATOR_MINUS:
			error("not implemented", 0, 0);
		case OPERATOR_PLUS:
			error("not implemented", 0, 0);
		case OPERATOR_DIVIDE:
			error("not implemented", 0, 0);
		case OPERATOR_MULTIPLY:
			error("not implemented", 0, 0);
		case OPERATOR_NOT:
			error("not implemented", 0, 0);
		case OPERATOR_OR:
			error("not implemented", 0, 0);
		case OPERATOR_AND:
			error("not implemented", 0, 0);
		case OPERATOR_MOD:
			error("not implemented", 0, 0);
		case OPERATOR_ASSIGN: {
			variable v = emit_expression(parent, right);

			switch (left->kind) {
			case EXPRESSION_VARIABLE: {
				opcode o;
				o.type = OPCODE_STORE_VARIABLE;
				o.size = OP_SIZE(o, op_store_var);
				o.op_store_var.from = v;
				// o.op_store_var.to = left->variable;
				emit_op(&o);
				break;
			}
			case EXPRESSION_MEMBER: {
				variable member_var = emit_expression(parent, left->member.left);

				opcode o;
				o.type = OPCODE_STORE_MEMBER;
				o.size = OP_SIZE(o, op_store_member);
				o.op_store_member.from = v;
				o.op_store_member.to = member_var;
				// o.op_store_member.member = left->member.right;
				emit_op(&o);
				break;
			}
			default:
				error("Expected a variable or a member", 0, 0);
			}

			return v;
		}
		}
		break;
	}
	case EXPRESSION_UNARY:
		switch (e->unary.op) {
		case OPERATOR_EQUALS:
			error("not implemented", 0, 0);
		case OPERATOR_NOT_EQUALS:
			error("not implemented", 0, 0);
		case OPERATOR_GREATER:
			error("not implemented", 0, 0);
		case OPERATOR_GREATER_EQUAL:
			error("not implemented", 0, 0);
		case OPERATOR_LESS:
			error("not implemented", 0, 0);
		case OPERATOR_LESS_EQUAL:
			error("not implemented", 0, 0);
		case OPERATOR_MINUS:
			error("not implemented", 0, 0);
		case OPERATOR_PLUS:
			error("not implemented", 0, 0);
		case OPERATOR_DIVIDE:
			error("not implemented", 0, 0);
		case OPERATOR_MULTIPLY:
			error("not implemented", 0, 0);
		case OPERATOR_NOT: {
			variable v = emit_expression(parent, e->unary.right);
			opcode o;
			o.type = OPCODE_NOT;
			o.size = OP_SIZE(o, op_not);
			o.op_not.from = v;
			o.op_not.to = allocate_variable(v.type);
			emit_op(&o);
			return o.op_not.to;
		}
		case OPERATOR_OR:
			error("not implemented", 0, 0);
		case OPERATOR_AND:
			error("not implemented", 0, 0);
		case OPERATOR_MOD:
			error("not implemented", 0, 0);
		case OPERATOR_ASSIGN:
			error("not implemented", 0, 0);
		}
	case EXPRESSION_BOOLEAN:
		error("not implemented", 0, 0);
	case EXPRESSION_NUMBER: {
		variable v = allocate_variable(f32_id);

		opcode o;
		o.type = OPCODE_LOAD_CONSTANT;
		o.size = OP_SIZE(o, op_load_constant);
		o.op_load_constant.number = 1.0f;
		o.op_load_constant.to = v;
		emit_op(&o);

		return v;
	}
	// case EXPRESSION_STRING:
	//	error("not implemented", 0, 0);
	case EXPRESSION_VARIABLE: {
		variable var = find_local_var(parent, e->variable);
		assert(var.index != 0);
		return var;
	}
	case EXPRESSION_GROUPING:
		error("not implemented", 0, 0);
	case EXPRESSION_CALL:
		error("not implemented", 0, 0);
	case EXPRESSION_MEMBER: {
		variable v = allocate_variable(e->type.type);

		opcode o;
		o.type = OPCODE_LOAD_MEMBER;
		o.size = OP_SIZE(o, op_load_member);
		assert(e->member.left->kind == EXPRESSION_VARIABLE);
		o.op_load_member.from = find_local_var(parent, e->member.left->variable);
		assert(o.op_load_member.from.index != 0);
		o.op_load_member.to = v;

		o.op_load_member.member_indices_size = 0;
		expression *right = e->member.right;
		struct_id prev_struct = e->member.left->type.type;
		structy *prev_s = get_struct(prev_struct);
		o.op_load_member.member_parent_type = prev_struct;

		while (right->kind == EXPRESSION_MEMBER) {
			assert(right->type.resolved && right->type.type != NO_STRUCT);
			assert(right->member.left->kind == EXPRESSION_VARIABLE);

			char *name = get_name(prev_s->name);
			bool found = false;
			for (size_t i = 0; i < prev_s->members.size; ++i) {
				if (prev_s->members.m[i].name == right->member.left->variable) {
					o.op_load_member.member_indices[o.op_load_member.member_indices_size] = (uint16_t)i;
					++o.op_load_member.member_indices_size;
					found = true;
					break;
				}
			}
			assert(found);

			prev_struct = right->member.left->type.type;
			prev_s = get_struct(prev_struct);
			right = right->member.right;
		}

		{
			assert(right->type.resolved && right->type.type != NO_STRUCT);
			assert(right->kind == EXPRESSION_VARIABLE);

			char *name = get_name(prev_s->name);
			bool found = false;
			for (size_t i = 0; i < prev_s->members.size; ++i) {
				if (prev_s->members.m[i].name == right->variable) {
					o.op_load_member.member_indices[o.op_load_member.member_indices_size] = (uint16_t)i;
					++o.op_load_member.member_indices_size;
					found = true;
					break;
				}
			}
			assert(found);
		}

		emit_op(&o);

		return v;
	}
	case EXPRESSION_CONSTRUCTOR:
		error("not implemented", 0, 0);
	}

	assert(false);
	variable v;
	v.index = 0;
	return v;
}

void emit_statement(block *parent, statement *statement) {
	switch (statement->kind) {
	case STATEMENT_EXPRESSION:
		emit_expression(parent, statement->expression);
		break;
	case STATEMENT_RETURN_EXPRESSION: {
		opcode o;
		o.type = OPCODE_RETURN;
		variable v = emit_expression(parent, statement->expression);
		if (v.index == 0) {
			o.size = offsetof(opcode, op_return);
		}
		else {
			o.size = OP_SIZE(o, op_return);
			o.op_return.var = v;
		}
		emit_op(&o);
		break;
	}
	case STATEMENT_IF:
		error("not implemented", 0, 0);
		break;
	case STATEMENT_BLOCK:
		error("not implemented", 0, 0);
		break;
	case STATEMENT_LOCAL_VARIABLE: {
		opcode o;
		o.type = OPCODE_VAR;
		o.size = OP_SIZE(o, op_var);
		if (statement->local_variable.init != NULL) {
			emit_expression(parent, statement->local_variable.init);
		}
		o.op_var.name = statement->local_variable.var.name;
		o.op_var.type = statement->local_variable.var.type.type;
		emit_op(&o);
		break;
	}
	}
}

void convert_function_block(struct statement *block) {
	if (block->kind != STATEMENT_BLOCK) {
		error("Expected a block", 0, 0);
	}
	for (size_t i = 0; i < block->block.vars.size; ++i) {
		variable var = allocate_variable(block->block.vars.v[i].type.type);
		block->block.vars.v[i].variable_id = var.index;
	}
	for (size_t i = 0; i < block->block.statements.size; ++i) {
		emit_statement(&block->block, block->block.statements.s[i]);
	}
}
