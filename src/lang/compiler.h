/*
 * This file is part of the Raven MUD.
 * Copyright (c) 2022  Eric Nijakowski
 *
 * See README and LICENSE for further information.
 */

#ifndef RAVEN_LANG_COMPILER_H
#define RAVEN_LANG_COMPILER_H

#include "../defs.h"
#include "../core/any.h"
#include "../core/blueprint.h"
#include "../core/vars.h"
#include "bytecodes.h"
#include "codewriter.h"

typedef t_cw_label t_compiler_label;

struct compiler {
  struct compiler*    parent;
  struct codewriter*  cw;
  struct blueprint*   bp;
  struct vars         vars;
  t_cw_label          break_label;
  t_cw_label          continue_label;
};

void compiler_create(struct compiler*   compiler,
                     struct codewriter* codewriter,
                     struct blueprint*  blueprint);
void compiler_create_sub(struct compiler* compiler, struct compiler* parent);
void compiler_destroy(struct compiler* compiler);

struct function* compiler_finish(struct compiler* compiler);

void compiler_add_arg(struct compiler* compiler,
                      struct type*     type,
                      struct symbol*   name);
void compiler_add_var(struct compiler* compiler,
                      struct type*     type,
                      struct symbol*   name);
void compiler_enable_varargs(struct compiler* compiler);

bool compiler_load_self(struct compiler* compiler);
bool compiler_load_constant(struct compiler* compiler, any value);
bool compiler_load_array(struct compiler* compiler, unsigned int size);
bool compiler_load_mapping(struct compiler* compiler, unsigned int size);
bool compiler_load_funcref(struct compiler* compiler, struct symbol* name);
bool compiler_load_var(struct compiler* compiler, struct symbol* name);
bool compiler_store_var(struct compiler* compiler, struct symbol* name);

void compiler_push_self(struct compiler* compiler);
void compiler_push(struct compiler* compiler);
void compiler_pop(struct compiler* compiler);

void compiler_op(struct compiler* compiler, enum raven_op op);

void compiler_send(struct compiler* compiler,
                   struct symbol*   message,
                   unsigned int     arg_count);
void compiler_super_send(struct compiler* compiler,
                         struct symbol*   message,
                         unsigned int     arg_count);

void compiler_return(struct compiler* compiler);

t_compiler_label compiler_open_label(struct compiler* compiler);
t_compiler_label compiler_open_break_label(struct compiler* compiler);
t_compiler_label compiler_open_continue_label(struct compiler* compiler);
void compiler_place_label(struct compiler* compiler, t_compiler_label label);
void compiler_close_label(struct compiler* compiler, t_compiler_label label);

void compiler_jump(struct compiler* compiler, t_compiler_label label);
void compiler_jump_if(struct compiler* compiler, t_compiler_label label);
void compiler_jump_if_not(struct compiler* compiler, t_compiler_label label);

void compiler_break(struct compiler* compiler);
void compiler_continue(struct compiler* compiler);

#endif
