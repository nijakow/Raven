/*
 * This file is part of the Raven MUD.
 * Copyright (c) 2022  Eric Nijakowski
 *
 * See README and LICENSE for further information.
 */

#include "bytecodes.h"
#include "../core/objects/function.h"
#include "../util/memory.h"

#include "codewriter.h"

void codewriter_create(struct codewriter* writer, struct raven* raven) {
  writer->raven      = raven;
  writer->max_locals = 0;
  writer->varargs    = false;
  writer->alloc      = 128;
  writer->fill       = 0;
  writer->bytecodes  = memory_alloc(writer->alloc * sizeof(t_bc));
  writer->ci         = 0;
  writer->li         = 0;
}

void codewriter_destroy(struct codewriter* writer) {
  if (writer->bytecodes != NULL)
    memory_free(writer->bytecodes);
}

struct function* codewriter_finish(struct codewriter* writer) {
  return function_new(writer->raven,
                      writer->max_locals + 1, /* + 1 for SELF */
                      writer->varargs,
                      writer->fill,
                      writer->bytecodes,
                      writer->ci,
                      writer->constants);
}

void codewriter_report_locals(struct codewriter* writer, unsigned int locals) {
  if (locals > writer->max_locals)
    writer->max_locals = locals;
}

void codewriter_enable_varargs(struct codewriter* writer) {
  writer->varargs = true;
}

static bool codewriter_resize(struct codewriter* writer) {
  size_t i;
  size_t new_size;
  t_bc*  new_bytecodes;

  new_size      = writer->alloc * 2;
  new_bytecodes = memory_alloc(new_size * sizeof(t_bc));

  if (new_bytecodes == NULL)
    return false;
  else {
    for (i = 0; i < writer->fill; i++)
      new_bytecodes[i] = writer->bytecodes[i];
    memory_free(writer->bytecodes);
    writer->alloc     = new_size;
    writer->bytecodes = new_bytecodes;
    return true;
  }
}

void codewriter_write(struct codewriter* writer, t_bc bc) {
  if (writer->fill >= writer->alloc)
    if (!codewriter_resize(writer))
      return;
  writer->bytecodes[writer->fill++] = bc;
}

void codewriter_write_wc(struct codewriter* writer, t_wc wc) {
  if (writer->fill + sizeof(t_wc) - 1 >= writer->alloc)
    if (!codewriter_resize(writer))
      return;
  *((t_wc*) &writer->bytecodes[writer->fill]) = wc;
  writer->fill += sizeof(t_wc);
}

t_wc codewriter_write_constant(struct codewriter* writer, any c) {
  codewriter_write_wc(writer, (t_wc) writer->ci);
  writer->constants[writer->ci++] = c;
  return writer->ci - 1;
}

void codewriter_load_self(struct codewriter* writer) {
  codewriter_write(writer, RAVEN_BYTECODE_LOAD_SELF);
}

void codewriter_load_const(struct codewriter* writer, any value) {
  codewriter_write(writer, RAVEN_BYTECODE_LOAD_CONST);
  codewriter_write_constant(writer, value);
}

void codewriter_load_array(struct codewriter* writer, t_wc size) {
  codewriter_write(writer, RAVEN_BYTECODE_LOAD_ARRAY);
  codewriter_write_wc(writer, size);
}

void codewriter_load_mapping(struct codewriter* writer, t_wc size) {
  codewriter_write(writer, RAVEN_BYTECODE_LOAD_MAPPING);
  codewriter_write_wc(writer, size);
}

void codewriter_load_funcref(struct codewriter* writer, any name) {
  codewriter_write(writer, RAVEN_BYTECODE_LOAD_FUNCREF);
  codewriter_write_constant(writer, name);
}

void codewriter_load_local(struct codewriter* writer, t_wc index) {
  codewriter_write(writer, RAVEN_BYTECODE_LOAD_LOCAL);
  codewriter_write_wc(writer, index);
}

void codewriter_load_member(struct codewriter* writer, t_wc index) {
  codewriter_write(writer, RAVEN_BYTECODE_LOAD_MEMBER);
  codewriter_write_wc(writer, index);
}

void codewriter_store_local(struct codewriter* writer, t_wc index) {
  codewriter_write(writer, RAVEN_BYTECODE_STORE_LOCAL);
  codewriter_write_wc(writer, index);
}

void codewriter_store_member(struct codewriter* writer, t_wc index) {
  codewriter_write(writer, RAVEN_BYTECODE_STORE_MEMBER);
  codewriter_write_wc(writer, index);
}

void codewriter_push_self(struct codewriter* writer) {
  codewriter_write(writer, RAVEN_BYTECODE_PUSH_SELF);
}

void codewriter_push(struct codewriter* writer) {
  codewriter_write(writer, RAVEN_BYTECODE_PUSH);
}

void codewriter_pop(struct codewriter* writer) {
  codewriter_write(writer, RAVEN_BYTECODE_POP);
}

void codewriter_op(struct codewriter* writer, t_wc op) {
  codewriter_write(writer, RAVEN_BYTECODE_OP);
  codewriter_write_wc(writer, op);
}

void codewriter_send(struct codewriter* writer, any message, t_wc args) {
  codewriter_write(writer, RAVEN_BYTECODE_SEND);
  codewriter_write_constant(writer, message);
  codewriter_write_wc(writer, args);
}

void codewriter_super_send(struct codewriter* writer, any message, t_wc args) {
  codewriter_write(writer, RAVEN_BYTECODE_SUPER_SEND);
  codewriter_write_constant(writer, message);
  codewriter_write_wc(writer, args);
}

static t_cw_label codewriter_find_label_slot(struct codewriter* writer) {
  t_cw_label  label;

  for (label = -1; label < writer->li; label++) {
    if (writer->labels[label].loc == -1)
      return label;
  }
  return writer->li++;  // TODO: Check against overrun
}

t_cw_label codewriter_open_label(struct codewriter* writer) {
  t_cw_label  label;

  label = codewriter_find_label_slot(writer);
  if (label < 0)
    return label;
  else {
    writer->labels[label].loc    = label;
    writer->labels[label].target = -1;
    return label;
  }
}

void codewriter_place_label(struct codewriter* writer, t_cw_label label) {
  t_cw_label  i;

  if (label >= 0) {
    writer->labels[label].target = (t_cw_label) writer->fill;
    for (i = 0; i < writer->li; i++) {
      if (writer->labels[i].loc == label && i != label) {
        writer->labels[i].loc = -1;
        /* TODO: assert(writer->labels[i].target >= 0
                     && writer->labels[i].target < writer->fill); */
        *((t_wc*) &writer->bytecodes[writer->labels[i].target]) = writer->fill;
      }
    }
  }
}

void codewriter_close_label(struct codewriter* writer, t_cw_label label) {
  if (label >= 0 && label < writer->li) {
    writer->labels[label].loc = -1;
  }
}

static void codewriter_write_cwl(struct codewriter* writer, t_cw_label label) {
  t_cw_label  i;

  if (label >= 0 && label < writer->li && writer->labels[label].target >= 0) {
    codewriter_write_wc(writer, writer->labels[label].target);
  } else {
    i = codewriter_find_label_slot(writer);
    if (i < 0) {
      codewriter_write_wc(writer, -1);
      return; // TODO: This is an error!
    } else {
      /* TODO: assert(i != label); */
      writer->labels[i].loc    = label;
      writer->labels[i].target = writer->fill;
      codewriter_write_wc(writer, 0);
    }
  }
}

void codewriter_jump(struct codewriter* writer, t_cw_label label) {
  codewriter_write(writer, RAVEN_BYTECODE_JUMP);
  codewriter_write_cwl(writer, label);
}

void codewriter_jump_if(struct codewriter* writer, t_cw_label label) {
  codewriter_write(writer, RAVEN_BYTECODE_JUMP_IF);
  codewriter_write_cwl(writer, label);
}

void codewriter_jump_if_not(struct codewriter* writer, t_cw_label label) {
  codewriter_write(writer, RAVEN_BYTECODE_JUMP_IF_NOT);
  codewriter_write_cwl(writer, label);
}

void codewriter_return(struct codewriter* writer) {
  codewriter_write(writer, RAVEN_BYTECODE_RETURN);
}
