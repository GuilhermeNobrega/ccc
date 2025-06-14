#include "compiler.h"
#include "helpers/vector.h"      /* LAB3: Incluir */
#include "cprocess.h"

struct compile_process* compile_process_create(const char* filename, const char* filename_out, int flags) {
    FILE* file = fopen(filename, "r");

    if (!file) {
        return NULL;
    }

    FILE* out_file = NULL;
    if (filename_out) {
        out_file = fopen(filename_out, "w");
        if (!out_file) {
            fclose(file);
            return NULL;
        }
    }

    struct compile_process* process = calloc(1, sizeof(struct compile_process));
    if (!process) {
        fclose(file);
        if (out_file) fclose(out_file);
        return NULL;
    }

    process->node_vec = vector_create(sizeof(struct node*));      /* LAB3: Inicializando o vetor */
    process->node_tree_vec = vector_create(sizeof(struct node*)); /* LAB3: Inicializando o vetor */

    if (!process->node_vec || !process->node_tree_vec) {
        fclose(file);
        if (out_file) fclose(out_file);
        free(process);
        return NULL;
    }

    process->flags = flags;
    process->cfile.fp = file;
    process->ofile = out_file;
    process->pos.line = 1;
    process->pos.col = 1;

    return process;
}
int compile_process_next_char(struct lex_process* lex_process) {
    struct compile_process* compiler = lex_process->compiler;
    return fgetc(compiler->cfile.fp);
}

int compile_process_peek_char(struct lex_process* lex_process) {
    struct compile_process* compiler = lex_process->compiler;
    int c = fgetc(compiler->cfile.fp);
    if (c != EOF) {
        ungetc(c, compiler->cfile.fp);
    }
    return c;
}


void compile_process_push_char(struct lex_process* lex_process, char c) {
    struct compile_process* compiler = lex_process->compiler;
    ungetc(c, compiler->cfile.fp);
}
