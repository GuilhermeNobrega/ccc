/* BEGIN - LAB 2 ----------------------------------*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "helpers/vector.h"
#include "helpers/buffer.h"
#include "compiler.h"

#define LEX_GETC_IF(buffer, c, exp) \
    for (c = peekc(); exp; c = peekc()){ \
        buffer_write(buffer, c); \
        nextc(); \
    }

struct token* read_next_token();
static struct lex_process* lex_process;
static struct token tmp_token;

const char* keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default",
    "do", "double", "else", "enum", "extern", "float", "for", "goto",
    "if", "int", "long", "register", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union",
    "unsigned", "void", "volatile", "while", NULL
};

int is_keyword(const char* str) {
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strcmp(str, keywords[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int peekc() {
    return lex_process->function->peek_char(lex_process);
}

static char nextc() {
    int c = lex_process->function->next_char(lex_process);
    lex_process->pos.col += 1;
    if (c == '\n') {
        lex_process->pos.line += 1;
        lex_process->pos.col = 1;
    }
    return c;
}

static void pushc(char c) {
    lex_process->function->push_char(lex_process, c);
}

static struct pos lex_file_position() {
    return lex_process->pos;
}

struct token* token_create(struct token* _token) {
    memcpy(&tmp_token, _token, sizeof(struct token));
    tmp_token.pos = lex_file_position();
    return &tmp_token;
}

static struct token* lexer_last_token() {
    return vector_back_or_null(lex_process->token_vec);
}

static struct token* handle_whitespace() {
    struct token* last_token = lexer_last_token();
    if (last_token) {
        last_token->whitespace = true;
    }
    nextc();
    return read_next_token();
}

unsigned long long read_number() {
    struct buffer* buffer = buffer_create();
    char c = peekc();

    if (c == '0') {
        buffer_write(buffer, nextc()); // pega o '0'
        c = peekc();

        if (c == 'x' || c == 'X') {
            buffer_write(buffer, nextc()); // pega o 'x'
            while (isxdigit(peekc())) {
                buffer_write(buffer, nextc());
            }
            buffer_write(buffer, 0x00);
            const char* hex = buffer_ptr(buffer);
            unsigned long long value = strtoull(hex, NULL, 16);
            printf("Token número hexadecimal: %s (dec: %llu)\n", hex, value);
            return value;
        } else if (c == 'b' || c == 'B') {
            buffer_write(buffer, nextc()); // pega o 'b'
            while (peekc() == '0' || peekc() == '1') {
                buffer_write(buffer, nextc());
            }
            buffer_write(buffer, 0x00);
            const char* bin = buffer_ptr(buffer);
            unsigned long long value = 0;
            for (int i = 2; bin[i] != '\0'; i++) {
                value = value * 2 + (bin[i] - '0');
            }
            printf("Token número binário: %s (dec: %llu)\n", bin, value);
            return value;
        } else {
            while (isdigit(peekc())) {
                buffer_write(buffer, nextc());
            }
            buffer_write(buffer, 0x00);
            const char* dec = buffer_ptr(buffer);
            unsigned long long value = strtoull(dec, NULL, 10);
            printf("Token número decimal: %s\n", dec);
            return value;
        }
    } else {
        while (isdigit(peekc())) {
            buffer_write(buffer, nextc());
        }
        buffer_write(buffer, 0x00);
        const char* dec = buffer_ptr(buffer);
        unsigned long long value = strtoull(dec, NULL, 10);
        printf("Token número decimal: %s\n", dec);
        return value;
    }
}

struct token* token_make_number_for_value(unsigned long number) {
    return token_create(&(struct token){.type=TOKEN_TYPE_NUMBER, .llnum=number});
}

struct token* token_make_number() {
    return token_make_number_for_value(read_number());
}


const char* read_string() {
    struct buffer* buffer = buffer_create();
    nextc(); // Skip opening quote
    char c = peekc();
    while (c != '"' && c != EOF) {
        buffer_write(buffer, c);
        nextc();
        c = peekc();
    }
    nextc(); // Skip closing quote
    buffer_write(buffer, 0x00);
    printf("Token string: \"%s\"\n", buffer->data);
    return buffer_ptr(buffer);
}

const char* read_comment() {
    struct buffer* buffer = buffer_create();
    char first = nextc();

    if (first == '/') {
        // Comentário de linha
        buffer_write(buffer, '/');
        buffer_write(buffer, '/');
        while (peekc() != '\n' && peekc() != EOF) {
            buffer_write(buffer, nextc());
        }
    } else if (first == '*') {
        // Comentário de bloco
        buffer_write(buffer, '/');
        buffer_write(buffer, '*');
        char c = peekc();
        char next;

        while (1) {
            if (c == EOF) break;

            c = nextc();
            buffer_write(buffer, c);

            if (c == '*' && peekc() == '/') {
                buffer_write(buffer, nextc()); // pega '/'
                break;
            }

            c = peekc();
        }
    } else {
        // Caso inesperado, como "*/" sem "/*"
        buffer_write(buffer, '/');
        buffer_write(buffer, first);
    }

    buffer_write(buffer, 0x00);
    printf("Token comentário: %s\n", buffer->data);
    return buffer_ptr(buffer);
}


const char* read_preprocessor_directive() {
    struct buffer* buffer = buffer_create();
    char c = peekc();
    while (c != '\n' && c != EOF) {
        buffer_write(buffer, c);
        nextc();
        c = peekc();
    }
    buffer_write(buffer, 0x00);
    printf("Token pré-processador: #%s\n", buffer->data);
    return buffer_ptr(buffer);
}

struct token* read_next_token() {
    struct token* token = NULL;
    char c = peekc();

    switch (c) {
        case EOF:
            return NULL;

        case ' ':
        case '\t':
            token = handle_whitespace();
            break;

        case '\n':
            nextc();
            token = token_create(&(struct token){.type=TOKEN_TYPE_NEWLINE});
            break;

        case ';': case '{': case '}': case '(': case ')': case ',': case '[': case ']':
            nextc();
            token = token_create(&(struct token){.type=TOKEN_TYPE_SYMBOL, .cval=c});
            printf("Token: %c\n", c);
            break;

       case '+': case '-': case '*': case '=':
       case '<': case '>': case '!': case '&': case '|': {
        char current = nextc(); // consome o caractere atual
        char op_str[2] = {current, '\0'}; // cria string com o operador
        token = token_create(&(struct token){
            .type = TOKEN_TYPE_OPERATOR,
            .cval = current,
            .sval = strdup(op_str) // atribui corretamente o sval
    });
    printf("Token: %c\n", current);
    break;
}


        case '/':
            nextc();
            if (peekc() == '/' || peekc() == '*') {
                token = token_create(&(struct token){
                    .type = TOKEN_TYPE_COMMENT,
                    .sval = read_comment()
                });
            } else {
                token = token_create(&(struct token){.type=TOKEN_TYPE_OPERATOR, .cval='/'});
            }
            break;

        case '"':
            token = token_create(&(struct token){.type=TOKEN_TYPE_STRING, .sval=read_string()});
            break;

            case '#':
            nextc(); // consome o '#'
            token = token_create(&(struct token){
                .type = TOKEN_TYPE_SYMBOL,
                .cval = '#'
            });
            printf("Token: %c\n", '#');
            vector_push(lex_process->token_vec, token); // adiciona '#' separado
        
            // Agora lemos a palavra que vem depois do #
            while (peekc() == ' ' || peekc() == '\t') nextc(); // pula espaços
            {
                struct buffer* buf = buffer_create();
                while (isalnum(peekc()) || peekc() == '_') {
                    buffer_write(buf, nextc());
                }
                buffer_write(buf, 0x00);
                const char* id = buffer_ptr(buf);
                token = token_create(&(struct token){.type=TOKEN_TYPE_IDENTIFIER, .sval=id});
                printf("Token: %s\n", id);
                vector_push(lex_process->token_vec, token); // adiciona o include por exemplo
            }
        
            // Agora o argumento, como <stdio.h> ou "meu.h"
            while (peekc() == ' ' || peekc() == '\t') nextc(); // pula espaços
            {
                struct buffer* buf = buffer_create();
                char c = peekc();
                if (c == '<') {
                    buffer_write(buf, nextc()); // escreve '<'
                    while ((c = peekc()) != '>' && c != EOF) {
                        buffer_write(buf, nextc());
                    }
                    if (peekc() == '>') buffer_write(buf, nextc()); // fecha com '>'
                } else if (c == '"') {
                    buffer_write(buf, nextc()); // escreve '"'
                    while ((c = peekc()) != '"' && c != EOF) {
                        buffer_write(buf, nextc());
                    }
                    if (peekc() == '"') buffer_write(buf, nextc()); // fecha com '"'
                } else {
                    // apenas lê como identificador
                    while (!isspace(peekc()) && peekc() != EOF) {
                        buffer_write(buf, nextc());
                    }
                }
                buffer_write(buf, 0x00);
                const char* arg = buffer_ptr(buf);
                token = token_create(&(struct token){.type=TOKEN_TYPE_IDENTIFIER, .sval=arg});
                printf("Token: %s\n", arg);
            }
            break;
        

        default:
            if (isdigit(c)) {
                token = token_make_number();
            } else if (isalpha(c) || c == '_') {
                struct buffer* buf = buffer_create();
                while (isalnum(peekc()) || peekc() == '_') {
                    buffer_write(buf, nextc());
                }
                buffer_write(buf, 0x00);
                const char* id = buffer_ptr(buf);
                if (is_keyword(id)) {
                    token = token_create(&(struct token){.type=TOKEN_TYPE_KEYWORD, .sval=id});
                    printf("Token palavra reservada: %s\n", id);
                } else {
                    token = token_create(&(struct token){.type=TOKEN_TYPE_IDENTIFIER, .sval=id});
                    printf("Token identificador: %s\n", id);
                }
            } else {
                printf("Caractere não reconhecido: '%c'\n", c);
                nextc();
            }
            break;
    }

    return token;
}

int lex(struct lex_process* process) {
    process->current_expression_count = 0;
    process->parentheses_buffer = NULL;
    lex_process = process;
    process->pos.filename = process->compiler->cfile.abs_path;

    struct token* token = read_next_token();
    while (token) {
        vector_push(process->token_vec, token);
        token = read_next_token();
    }

    return LEXICAL_ANALYSIS_ALL_OK;
}
/* END - LAB 2 ------------------------------------*/
