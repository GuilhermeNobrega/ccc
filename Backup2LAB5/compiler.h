#ifndef COMPILER_H
#define COMPILER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> // Se necessário para memcpy

extern struct vector* node_vector;
extern struct vector* node_vector_root;

enum {
    SYMBOL_TYPE_NODE,
    SYMBOL_TYPE_NATIVE_FUNCTION,
    SYMBOL_TYPE_UNKNOWN
};

struct symbol {
    const char* name;
    int type;
    void* data;
};

struct scope {
    int flags;
    struct vector* entities;     // ponteiro para vetor de símbolos
    size_t size;                 // tamanho total do escopo em bytes
    struct scope* parent;       // escopo pai (NULL se for global)
};

enum {
DATATYPE_FLAG_IS_SIGNED = 0b00000001,
DATATYPE_FLAG_IS_STATIC = 0b00000010,
DATATYPE_FLAG_IS_CONST = 0b00000100,
DATATYPE_FLAG_IS_POINTER = 0b00001000,
DATATYPE_FLAG_IS_ARRAY = 0b00010000,
DATATYPE_FLAG_IS_EXTERN = 0b00100000,
DATATYPE_FLAG_IS_RESTRICT = 0b01000000,
DATATYPE_FLAG_IS_IGNORE_TYPE_CHECKING = 0b10000000,
DATATYPE_FLAG_IS_SECONDARY = 0b100000000,
DATATYPE_FLAG_IS_STRUCT_UNION_NO_NAME = 0b1000000000,
DATATYPE_FLAG_IS_LETERAL = 0b10000000000
};
enum {
DATATYPE_VOID,
DATATYPE_CHAR,
DATATYPE_SHORT,
DATATYPE_INTEGER,
DATATYPE_LONG,
DATATYPE_FLOAT,
DATATYPE_DOUBLE,
DATATYPE_STRUCT,
DATATYPE_UNION,
DATATYPE_UNKNOWN
};
enum {
DATATYPE_EXPECT_PRIMITIVE,
DATATYPE_EXPECT_UNION,
DATATYPE_EXPECT_STRUCT
};
struct token* token_expect_identifier();
struct token* token_next();

struct datatype {
int flags;
// EX: long, int, float, etc.
int type;
const char* type_str;
// EX: long int, sendo int o secundário.
struct datatype* datatype_secondary;
// Tamanho do datatype. EX: long tem 8 bytes.
size_t size;
// Quantidades de ponteiros alinhados. Ex: int** a, pointer_depth == 2.
int pointer_depth;
union {
struct node* struct_node;
struct node* union_node;
};
};
// Declarações antecipadas para evitar erros de tipo incompleto
struct compile_process;
struct token;
struct node;
struct vector;
struct buffer;

/* ----------------- LAB 2 ----------------- */

struct pos {
    int line;
    int col;
    const char* filename;
};

#define TOKEN_TYPE_PREPROCESSOR 10
#define S_EQ(a, b) (strcmp((a), (b)) == 0)


#define NUMERIC_CASE \
    case '0': \
    case '1': \
    case '2': \
    case '3': \
    case '4': \
    case '5': \
    case '6': \
    case '7': \
    case '8': \
    case '9'

enum {
    LEXICAL_ANALYSIS_ALL_OK,
    LEXICAL_ANALYSIS_IMPUT_ERROR
};

enum {
    TOKEN_TYPE_KEYWORD,
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_OPERATOR,
    TOKEN_TYPE_SYMBOL,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_COMMENT,
    TOKEN_TYPE_NEWLINE
};

// Flags para nós sintáticos (LAB3)
enum {
    NODE_FLAG_INSIDE_EXPRESSION = 0b00000001
};

// ---------------- EXPRESSÕES E PRECEDÊNCIA ----------------

#define TOTAL_OPERATOR_GROUPS 14
#define MAX_OPERATORS_IN_GROUP 12

enum {
    ASSOCIATIVITY_LEFT_TO_RIGHT,
    ASSOCIATIVITY_RIGHT_TO_LEFT
};

struct expressionable_op_precedence_group {
    const char* operators[MAX_OPERATORS_IN_GROUP];
    int associativity;
};

extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS];

/* --- FUNÇÕES parser.c --- */
int parse(struct compile_process* process);
int parse_next();

/* --- FUNÇÕES token.c --- */
bool token_is_keyword(struct token* token, const char* value);
bool token_is_symbol(struct token* token, const char value);
bool discart_token(struct token* token);

/* --- FUNÇÕES node.c --- */
void node_set_vector(struct vector* vec, struct vector* root_vec);
void node_push(struct node* node);
struct node* node_peek_or_null();
struct node* node_peek();
struct node* node_pop();
struct node* node_peek_expressionable_or_null();
bool node_is_expressionable(struct node* node);
void make_exp_node(struct node* node_left, struct node* node_right, const char* op);
struct node* node_create(struct node* _node);

// Estrutura de token
struct token {
    int type;
    int flags;
    struct pos pos;

    union {
        char cval;
        const char* sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum;
        void* any;
    };

    bool whitespace;
    const char* between_brackets;
};

struct lex_process;

typedef int (*LEX_PROCESS_NEXT_CHAR)(struct lex_process* process);
typedef int (*LEX_PROCESS_PEEK_CHAR)(struct lex_process* process);
typedef void (*LEX_PROCESS_PUSH_CHAR)(struct lex_process* process, char c);
typedef char (*LEX_PROCESS_PEEK_AHEAD)(struct lex_process* process, int offset);

struct lex_process_functions {
    LEX_PROCESS_NEXT_CHAR next_char;
    LEX_PROCESS_PEEK_CHAR peek_char;
    LEX_PROCESS_PUSH_CHAR push_char;
    LEX_PROCESS_PEEK_AHEAD peek_ahead;
};

struct lex_process {
    struct pos pos;
    struct vector* token_vec;
    struct compile_process* compiler;
    int current_expression_count;
    struct buffer* parentheses_buffer;
    struct lex_process_functions* function;
    void* private;
    const char* input;
    int input_size;
};

// cprocess.c
int compile_process_next_char(struct lex_process* lex_process);
int compile_process_peek_char(struct lex_process* lex_process);
void compile_process_push_char(struct lex_process* lex_process, char c);

// lex_process.c
struct lex_process* lex_process_create(struct compile_process* compiler, struct lex_process_functions* functions, void* private);
void lex_process_free(struct lex_process* process);
void* lex_process_private(struct lex_process* process);
struct vector* lex_process_tokens(struct lex_process* process);

// lexer.c
int lex(struct lex_process* process);

// compiler.c
void compiler_error(struct compile_process* compiler, const char* msg, ...);
void compiler_warning(struct compile_process* compiler, const char* msg, ...);

/* ----------------- LAB 3 ----------------- */

enum {
    COMPILER_FILE_COMPILED_OK,
    COMPILER_FAILED_WITH_ERRORS
};

struct compile_process {
    int flags;

    // Arquivo de entrada
    struct pos pos;
    struct compile_process_input_file {
        FILE* fp;
        const char* abs_path;
    } cfile;

    // LAB3: Vetores de tokens e AST
    struct vector* token_vec;
    struct vector* node_vec;
    struct vector* node_tree_vec;

    FILE* ofile;

    // Escopos atuais do compilador
    struct {
        struct scope* root;
        struct scope* current;
    } scope;

    // Tabelas de símbolos
    struct {
        struct vector* table;
        struct vector* tables;
    } symbols;
};


enum {
    NODE_TYPE_EXPRESSION,
    NODE_TYPE_EXPRESSION_PARENTHESES,
    NODE_TYPE_NUMBER,
    NODE_TYPE_IDENTIFIER,
    NODE_TYPE_STRING,
    NODE_TYPE_VARIABLE,
    NODE_TYPE_VARIABLE_LIST,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_BODY,
    NODE_TYPE_STATEMENT_RETURN,
    NODE_TYPE_STATEMENT_IF,
    NODE_TYPE_STATEMENT_ELSE,
    NODE_TYPE_STATEMENT_WHILE,
    NODE_TYPE_STATEMENT_DO_WHILE,
    NODE_TYPE_STATEMENT_FOR,
    NODE_TYPE_STATEMENT_BREAK,
    NODE_TYPE_STATEMENT_CONTINUE,
    NODE_TYPE_STATEMENT_SWITCH,
    NODE_TYPE_STATEMENT_CASE,
    NODE_TYPE_STATEMENT_DEFAULT,
    NODE_TYPE_STATEMENT_GOTO,
    NODE_TYPE_UNARY,
    NODE_TYPE_TENARY,
    NODE_TYPE_LABEL,
    NODE_TYPE_STRUCT,
    NODE_TYPE_UNION,
    NODE_TYPE_BRACKET,
    NODE_TYPE_CAST,
    NODE_TYPE_BLANK,
};

enum {
    PARSE_ALL_OK,
    PARSE_GENERAL_ERROR
};

struct datatype;  // Forward declaration

struct node {
    int type;
    int flags;
    struct pos pos;

    struct node_binded {
        struct node* owner;
        struct node* funtion; // (apesar do typo, mantenha igual ao código que já funciona)
    } binded;

    union {
        char cval;
        const char* sval;
        unsigned int inum;
        unsigned long lnum;
        unsigned long long llnum;
        void* any;
    };

    union {
        struct exp {
            struct node* left;
            struct node* right;
            const char* op;
        } exp;

        struct var {
            struct datatype type;
            const char* name;
            struct node* val;
        } var;

        struct varlist {
            struct vector* list;
        } var_list;
    };
};


// compiler.c
bool token_is_operator(struct token* token, const char* val);

int compile_file(const char* filename, const char* out_filename, int flags);

void print_node(struct node* node, int indent);
void print_node_tree(struct vector* node_tree_vec, int indent);
void print_node_tree_single(struct node* n, int indent);
#endif // COMPILER_H
