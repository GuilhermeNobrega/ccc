#ifndef CPROCESS_H
#define CPROCESS_H

#include "compiler.h"

// Declarações das funções implementadas em cprocess.c
struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags);
int compile_process_next_char(struct lex_process* lex_process);
int compile_process_peek_char(struct lex_process* lex_process);
void compile_process_push_char(struct lex_process* lex_process, char c);

#endif // CPROCESS_H
