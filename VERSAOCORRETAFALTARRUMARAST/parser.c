#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h> // LAB4

struct history {
    int flags;
}; // só se não estiver definida em header
struct history* history_begin(int flags);  // <- adiciona esse protótipo
// Parte 2 - Prototypes
void parse_expressionable(struct history* history);
void make_variable_node_and_register(struct history* history, struct datatype* dtype, struct token* name_token, struct node* value_node);
void make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node);
static bool token_next_is_operator(const char* op);

// Funções auxiliares que ainda não têm protótipo
struct token* token_next();
struct token* token_peek_next();
void parse_single_token_to_node();
static struct compile_process* current_process;
static struct token* parser_last_token;
extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS]; // LAB4
void parser_reorder_expression(struct node** pnode); // LAB4
void parser_node_shift_children_left(struct node* node); // LAB4

//LAB5

static bool keyword_is_datatype(const char* val);
static bool is_keyword_variable_modifier(const char* val);
bool token_is_primitive_keyword(struct token* token);
void parser_get_datatype_tokens(struct token** datatype_token, struct token** datatype_secundary_token);
int parser_datatype_expected_for_type_string(const char* str);
int parser_get_random_type_index();
struct token* parser_build_random_type_name();
int parser_get_pointer_depth();
bool parser_datatype_is_secondary_allowed_for_type(const char* type);
void parser_datatype_adjust_size_for_secondary(struct datatype* datatype, struct token* datatype_secondary_token);
void parser_datatype_init_type_and_size_for_primitive(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out);
void parser_datatype_init_type_and_size(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type);
void parser_datatype_init(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type);
void parse_datatype_type(struct datatype* dtype);
void parse_datatype_modifiers(struct datatype* dtype);
void parse_datatype(struct datatype* dtype);
void parse_variable_function_or_struct_union(struct history* history);
void parse_keyword(struct history *history);
//LAB5


void make_variable_node_and_register(struct history* history, struct datatype* dtype, struct token* name_token, struct node* value_node) {
    struct node* var_node = node_create(&(struct node){
        .type = NODE_TYPE_VARIABLE,
        .var.name = name_token->sval,
        .var.val = value_node,
        .var.type = *dtype
    });
    //printf("DEBUG: [make_variable_node_and_register] Criando nó para variável: %s\n", name_token->sval);

    // Empilha no vetor global de nós
    node_push(var_node);

    // Registra na raiz da árvore
    vector_push(node_vector_root, &var_node);
}

void parse_datatype(struct datatype* dtype) { // LAB5
    memset(dtype, 0, sizeof(struct datatype));

    // Flag padrão: signed
    dtype->flags |= DATATYPE_FLAG_IS_SIGNED;

    // Aplica modificadores antes e depois do tipo
    parse_datatype_modifiers(dtype);
    parse_datatype_type(dtype);
    parse_datatype_modifiers(dtype);
}


void parse_keyword(struct history* history) {
    struct token* datatype_token = token_next();  // Consome o 'int' ou 'float'
    struct datatype dtype;

    parser_datatype_init_type_and_size_for_primitive(datatype_token, NULL, &dtype);
    struct vector* var_list = vector_create(sizeof(struct node*));

    // Criar lista de variáveis
    parser_datatype_init_type_and_size_for_primitive(datatype_token, NULL, &dtype);

    while (1) {
        struct token* name_token = token_next();  // Nome da variável

        // Verifica inicialização (ex: b = 2)
        struct node* value_node = NULL;
        struct token* next = token_peek_next();
        if (next && next->type == TOKEN_TYPE_OPERATOR && strcmp(next->sval, "=") == 0) {
            token_next();  // consome '='
            parse_expressionable(history);
            value_node = node_pop();
        }

        // Cria o nó de variável
        struct node* var_node = node_create(&(struct node){
            .type = NODE_TYPE_VARIABLE,
            .var.name = name_token->sval,
            .var.val = value_node,
            .var.type = dtype
        });

        vector_push(var_list, &var_node);

        struct token* sep = token_peek_next();

                
        if (!sep) {
            // Se não houver mais tokens, assume que a declaração terminou corretamente.
            break;
        }

        if (sep->type == TOKEN_TYPE_SYMBOL && sep->cval == ',') {
            token_next(); // consome vírgula
            continue;
        } else if (sep->type == TOKEN_TYPE_SYMBOL && sep->cval == ';') {
            token_next(); // consome ponto e vírgula
            break;
        } else {
            compiler_error(current_process, "Esperava ',' ou ';' após variável.");
        }

    }

    // Cria um nó de lista de variáveis
    struct node* var_list_node = node_create(&(struct node){
        .type = NODE_TYPE_VARIABLE_LIST,
        .var_list.list = var_list
    });

    node_push(var_list_node);
    vector_push(node_vector_root, &var_list_node);
}

void parse_expressionable_root(struct history* history) { // LAB5 - Parte 2
    parse_expressionable(history);

    struct node* result_node = node_pop();
    node_push(result_node);
}

void parse_variable(struct datatype* dtype, struct token* name_token, struct history* history) {
    struct vector* var_list = vector_create(sizeof(struct node*));

    while (1) {
        struct node* value_node = NULL;

        // Verifica se há atribuição
        if (token_next_is_operator("=")) {
            token_next(); // consome "="
            parse_expressionable_root(history);
            value_node = node_pop();
        }

        // Cria e registra o nó da variável individual
        make_variable_node_and_register(history, dtype, name_token, value_node);
        struct node* var_node = node_pop();

        // Adiciona o node individual à lista
        vector_push(var_list, &var_node);

        // Verifica se a próxima é uma vírgula
        if (token_next_is_operator(",")) {
            token_next(); // consome ","
            name_token = token_expect_identifier();
            continue;
        }

        // Verifica se terminou com ponto-e-vírgula
        if (token_next_is_operator(";")) {
            token_next(); // consome ";"
            break;
        }

        // Se não for vírgula nem ponto-e-vírgula, é erro
        printf("Erro: esperava ',' ou ';' após variável.\n");
        exit(1);
    }

    // Criamos agora o node de tipo LISTA DE VARIÁVEIS
    struct node* var_list_node = node_create(&(struct node){
        .type = NODE_TYPE_VARIABLE_LIST,
        .var_list.list = var_list
    });

    node_push(var_list_node);
}



void make_variable_node(struct datatype* dtype, struct token* name_token, struct node* value_node) { // LAB5 - Parte 2
    const char* name_str = NULL;
    if (name_token) name_str = name_token->sval;

    node_create(&(struct node){
        .type = NODE_TYPE_VARIABLE,
        .var.name = name_str,
        .var.type = *dtype,
        .var.val = value_node
    });
}

void parse_variable_function_or_struct_union(struct history* history) { // LAB5 - Parte 2
    struct datatype dtype;
    parse_datatype(&dtype);

    struct token* name_token = token_next();
    if (name_token->type != TOKEN_TYPE_IDENTIFIER) {
        compiler_error(current_process, "Variavel declarada sem nome!\n");
    }

    // TODO: Verificar se eh uma declaracao de funcao. Ex: "int a()".

    parse_variable(&dtype, name_token, history);
    
}

void parse_datatype_modifiers(struct datatype* dtype) { // LAB5
    struct token* token = token_peek_next();

    while (token && token->type == TOKEN_TYPE_KEYWORD) {
        if (!is_keyword_variable_modifier(token->sval)) break;

        if (S_EQ(token->sval, "signed")) {
            dtype->flags |= DATATYPE_FLAG_IS_SIGNED;
        } else if (S_EQ(token->sval, "unsigned")) {
            dtype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
        } else if (S_EQ(token->sval, "static")) {
            dtype->flags |= DATATYPE_FLAG_IS_STATIC;
        } else if (S_EQ(token->sval, "const")) {
            dtype->flags |= DATATYPE_FLAG_IS_CONST;
        } else if (S_EQ(token->sval, "extern")) {
            dtype->flags |= DATATYPE_FLAG_IS_EXTERN;
        } else if (S_EQ(token->sval, "__ignore_typecheck__")) {
            dtype->flags |= DATATYPE_FLAG_IS_IGNORE_TYPE_CHECKING;
        }

        token_next();
        token = token_peek_next();
    }
}


void parse_datatype_type(struct datatype* dtype) { // LAB5
    struct token* datatype_token = NULL;
    struct token* datatype_secondary_token = NULL;
    parser_get_datatype_tokens(&datatype_token, &datatype_secondary_token);

    int expected_type = parser_datatype_expected_for_type_string(datatype_token->sval);

    if (S_EQ(datatype_token->sval, "union") || S_EQ(datatype_token->sval, "struct")) {
        // Caso da struct/union com nome explícito
        if (token_peek_next()->type == TOKEN_TYPE_IDENTIFIER) {
            datatype_token = token_next();
        } else {
            // Struct/union sem nome → nome gerado automaticamente
            datatype_token = parser_build_random_type_name();
            dtype->flags |= DATATYPE_FLAG_IS_STRUCT_UNION_NO_NAME;
        }
    }

    // Quantidade de ponteiros
    int pointer_depth = parser_get_pointer_depth();

    parser_datatype_init(datatype_token, datatype_secondary_token, dtype, pointer_depth, expected_type);
}

void parser_datatype_init_type_and_size(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type) {
    if (!(expected_type == DATATYPE_EXPECT_PRIMITIVE) && datatype_secondary_token) {
        compiler_error(current_process, "Você utilizou um datatype secundário inválido!\n");
    }

    switch (expected_type) {
        case DATATYPE_EXPECT_PRIMITIVE:
            parser_datatype_init_type_and_size_for_primitive(datatype_token, datatype_secondary_token, datatype_out);
            break;
        case DATATYPE_EXPECT_STRUCT:
        case DATATYPE_EXPECT_UNION:
            compiler_error(current_process, "Struct e Unions ainda não estão implementados!\n");
            break;
        default:
            compiler_error(current_process, "BUG: Erro desconhecido!\n");
    }
}

void parser_datatype_init(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out, int pointer_depth, int expected_type) {
    parser_datatype_init_type_and_size(datatype_token, datatype_secondary_token, datatype_out, pointer_depth, expected_type);
    datatype_out->type_str = datatype_token->sval;
}


static bool keyword_is_datatype(const char* val) { // LAB5
    return S_EQ(val, "void") ||
           S_EQ(val, "char") ||
           S_EQ(val, "int") ||
           S_EQ(val, "short") ||
           S_EQ(val, "float") ||
           S_EQ(val, "double") ||
           S_EQ(val, "long") ||
           S_EQ(val, "struct") ||
           S_EQ(val, "union");
}

static bool is_keyword_variable_modifier(const char* val) { // LAB5
    return S_EQ(val, "unsigned") ||
           S_EQ(val, "signed") ||
           S_EQ(val, "static") ||
           S_EQ(val, "const") ||
           S_EQ(val, "extern") ||
           S_EQ(val, "__ignore_typecheck__");
}

// LAB5 - Verifica se um token keyword é um tipo primitivo
bool token_is_primitive_keyword(struct token* token) {
    if (!token) return false;
    if (token->type != TOKEN_TYPE_KEYWORD) return false;

    if (S_EQ(token->sval, "void")) return true;
    else if (S_EQ(token->sval, "char")) return true;
    else if (S_EQ(token->sval, "short")) return true;
    else if (S_EQ(token->sval, "int")) return true;
    else if (S_EQ(token->sval, "long")) return true;
    else if (S_EQ(token->sval, "float")) return true;
    else if (S_EQ(token->sval, "double")) return true;

    return false;
}

// LAB5 - Retorna o tipo esperado com base no texto ("union", "struct", etc)
int parser_datatype_expected_for_type_string(const char* str) {
    int type = DATATYPE_EXPECT_PRIMITIVE;

    if (S_EQ(str, "union")) {
        type = DATATYPE_EXPECT_UNION;
    } else if (S_EQ(str, "struct")) {
        type = DATATYPE_EXPECT_STRUCT;
    }

    return type;
}

// LAB5 - Gera índice incremental único para nomes fictícios
int parser_get_random_type_index() {
    static int x = 0;
    x++;
    return x;
}

// LAB5 - Gera um token com nome de tipo aleatório ex: customtypename_1
struct token* parser_build_random_type_name() {
    char tmp_name[25];
    sprintf(tmp_name, "customtypename_%i", parser_get_random_type_index());

    char* sval = malloc(sizeof(tmp_name));
    strncpy(sval, tmp_name, sizeof(tmp_name));

    struct token* token = calloc(1, sizeof(struct token));
    token->type = TOKEN_TYPE_IDENTIFIER;
    token->sval = sval;

    return token;
}

// LAB5 - Trata dois datatypes seguidos, ex: long int a;
void parser_get_datatype_tokens(struct token** datatype_token, struct token** datatype_secondary_token) {
    *datatype_token = token_next();
    struct token* next_token = token_peek_next();

    if (token_is_primitive_keyword(next_token)) {
        *datatype_secondary_token = next_token;
        token_next();
    }
}

// LAB5 - Conta a profundidade de ponteiros: ex: int*** a → retorna 3
int parser_get_pointer_depth() {
    int depth = 0;
    struct token* token = NULL;

    while ((token = token_peek_next()) &&
           (token->type == TOKEN_TYPE_OPERATOR) &&
           S_EQ(token->sval, "*")) {
        depth++;
        token_next();
    }

    return depth;
}

// LAB5 - Verifica se um tipo secundário pode ser usado junto ao primário
bool parser_datatype_is_secondary_allowed_for_type(const char* type) {
    return S_EQ(type, "long") || S_EQ(type, "short") ||
           S_EQ(type, "double") || S_EQ(type, "float");
}
void parser_datatype_init_type_and_size_for_primitive(struct token* datatype_token, struct token* datatype_secondary_token, struct datatype* datatype_out) {
    if (!parser_datatype_is_secondary_allowed_for_type(datatype_token->sval) && datatype_secondary_token) {
        compiler_error(current_process, "Você utilizou um datatype secundário inválido!\n");
    }

    if (S_EQ(datatype_token->sval, "void")) {
        datatype_out->type = DATATYPE_VOID;
        datatype_out->size = 0;
    } else if (S_EQ(datatype_token->sval, "char")) {
        datatype_out->type = DATATYPE_CHAR;
        datatype_out->size = 1; // 1 BYTE
    } else if (S_EQ(datatype_token->sval, "short")) {
        datatype_out->type = DATATYPE_SHORT;
        datatype_out->size = 2; // 2 BYTES
    } else if (S_EQ(datatype_token->sval, "int")) {
        datatype_out->type = DATATYPE_INTEGER;
        datatype_out->size = 4; // 4 BYTES
    } else if (S_EQ(datatype_token->sval, "long")) {
        datatype_out->type = DATATYPE_LONG;
        datatype_out->size = 8; // 8 BYTES
    } else if (S_EQ(datatype_token->sval, "float")) {
        datatype_out->type = DATATYPE_FLOAT;
        datatype_out->size = 4; // 4 BYTES
    } else if (S_EQ(datatype_token->sval, "double")) {
        datatype_out->type = DATATYPE_DOUBLE;
        datatype_out->size = 8; // 8 BYTES
    } else {
        compiler_error(current_process, "BUG: Datatype inválido!\n");
    }

    parser_datatype_adjust_size_for_secondary(datatype_out, datatype_secondary_token);
}


// LAB5 - Ajusta o tamanho e flags do tipo, considerando o tipo secundário
void parser_datatype_adjust_size_for_secondary(struct datatype* datatype, struct token* datatype_secondary_token) {
    if (!datatype_secondary_token) return;

    struct datatype* secondary_data_type = calloc(1, sizeof(struct datatype));
    parser_datatype_init_type_and_size_for_primitive(datatype_secondary_token, NULL, secondary_data_type);
    datatype->size += secondary_data_type->size;
    datatype->datatype_secondary = secondary_data_type;
    datatype->flags |= DATATYPE_FLAG_IS_SECONDARY;
}


void parse_expressionable(struct history* history);
int parse_expressionable_single(struct history* history);
static bool parser_left_op_has_priority(const char* op_left, const char* op_right); // LAB4
void parser_reorder_expression(struct node** pnode); // LAB4

// Inicia um novo histórico com flags.
struct history* history_begin(int flags) {
    struct history* history = calloc(1, sizeof(struct history));
    history->flags = flags;
    return history;
}

// Cria um novo histórico baseado em outro.
struct history* history_down(struct history* history, int flags) {
    struct history* new_history = calloc(1, sizeof(struct history));
    memcpy(new_history, history, sizeof(struct history));
    new_history->flags = flags;
    return new_history;
}

// Ignora tokens do tipo comentário ou nova linha.
static void parser_ignore_nl_or_comment(struct token* token) {
    while (token && discart_token(token)) {
        // Pula o token que deve ser descartado no vetor.
        vector_peek(current_process->token_vec);
        // Pega o próximo token para ver se ele também será descartado.
        token = vector_peek_no_increment(current_process->token_vec);
    }
}

// Recupera o próximo token, pulando comentários e novas linhas.
    struct token* token_next() {
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    current_process->pos = next_token->pos; // Atualiza a posição do arquivo de compilação.
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}
static bool token_next_is_operator(const char* op) { // LAB5 - Parte 2
struct token* token = token_peek_next();
return token_is_operator(token, op);
}

// Apenas visualiza o próximo token sem consumir.
struct token* token_peek_next() {
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    return vector_peek_no_increment(current_process->token_vec);
}
void parse_identifier(struct history* history) { // LAB5
    struct token* token = token_peek_next();
    if (!token || token->type != TOKEN_TYPE_IDENTIFIER) {
        compiler_error(current_process, "Esperado identificador após declaração.");
    }

    parse_single_token_to_node();
}

void parse_keyword_for_global() {
    // Inicializa os vetores de node
    node_set_vector(current_process->node_vec, current_process->node_tree_vec);
    parse_keyword(history_begin(0));
    struct node* node = node_pop();
    vector_push(current_process->node_tree_vec, &node);//////

}


// Converte um token simples diretamente para nó.
void parse_single_token_to_node() {
    struct token* token = token_next();
    struct node* node = NULL;

    switch (token->type) {
        case TOKEN_TYPE_NUMBER:
            node = node_create(&(struct node){ .type = NODE_TYPE_NUMBER, .llnum = token->llnum });
            break;
        case TOKEN_TYPE_IDENTIFIER:
            node = node_create(&(struct node){ .type = NODE_TYPE_IDENTIFIER, .sval = token->sval });
            break;
        case TOKEN_TYPE_STRING:
            node = node_create(&(struct node){ .type = NODE_TYPE_STRING, .sval = token->sval });
            break;
        default:
            compiler_error(current_process, "Esse token não pode ser convertido para node!\n");
            break;
    }
}

// Inicia análise de expressão com operador.
void parse_expressionable_for_op(struct history* history, const char* op) {
    parse_expressionable(history);
}

void parse_exp_normal(struct history* history) {
    struct token* op_token = token_peek_next();
    const char* op = op_token->sval;


    struct node* node_left = node_peek_expressionable_or_null();
    if (!node_left) return;

    // Retira da lista de tokens o token de operador. Ex: "123+456", retira o token "+".
    token_next();

    // Retira da lista de nodes, o último node inserido.
    node_pop();

    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    // Nesse momento, temos o node da esquerda e o operador.
    // Essa função irá criar o node da direita.
    parse_expressionable_for_op(history_down(history, history->flags), op);

    struct node* node_right = node_pop();
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    // Cria o node de expressão, passando node esquerda, node direita e operador.
    make_exp_node(node_left, node_right, op);

    struct node* exp_node = node_pop();

    // LAB4: Inserir aqui o código para reordenar a expressão (com base na precedência)
    parser_reorder_expression(&exp_node); // LAB4

    node_push(exp_node);
}

// Inicia o parsing de uma expressão com operador.
int parse_exp(struct history* history) {
    parse_exp_normal(history);
    return 0;
}

// Analisa um único token que pode fazer parte de uma expressão.
int parse_expressionable_single(struct history* history) {
    struct token* token = token_peek_next();
    if (!token) return -1;

    if (token->type == TOKEN_TYPE_KEYWORD || token->type == TOKEN_TYPE_IDENTIFIER || token->type == TOKEN_TYPE_OPERATOR) {
        printf("DEBUG: [parse_expressionable_single] Token válido => tipo: %d, sval: %s\n",
               token->type, token->sval ? token->sval : "NULL");
    } else {
        printf("DEBUG: [parse_expressionable_single] Token simbólico => tipo: %d, cval: '%c'\n",
               token->type, token->cval);
    }

    int res = -1;

    switch (token->type) {
        case TOKEN_TYPE_NUMBER:
            //printf("DEBUG: Entrou no case TOKEN_TYPE_NUMBER\n");
            parse_single_token_to_node();
            res = 0;
            break;

        case TOKEN_TYPE_IDENTIFIER:
            //printf("DEBUG: Entrou no case TOKEN_TYPE_IDENTIFIER\n");
            parse_identifier(history);
            res = 0;
            break;

        case TOKEN_TYPE_KEYWORD:
            //printf("DEBUG: Entrou no case TOKEN_TYPE_KEYWORD\n");
            parse_keyword(history);
            res = 0;
            break;

        case TOKEN_TYPE_OPERATOR:
            //printf("DEBUG: Entrou no case TOKEN_TYPE_OPERATOR\n");
            parse_exp(history);
            res = 0;
            break;

        case TOKEN_TYPE_SYMBOL:
            if (token->cval == '(') {
                token_next(); // consome '('

                // Começa subexpressão recursiva
                parse_expressionable(history);

                // Confirma se empilhou algo
                struct node* inner = node_peek_expressionable_or_null();
                if (!inner) {
                    compiler_error(current_process, "Expressão entre parênteses inválida ou vazia!\n");
                }

                // Remove com segurança
                inner = node_pop();

                // Verifica se fechou corretamente
                struct token* closing = token_next();
                if (!closing || closing->cval != ')') {
                    compiler_error(current_process, "Parêntese de fechamento ')' esperado!\n");
                }

                // Cria nó com subexpressão agrupada
                struct node* paren_node = node_create(&(struct node){
                    .type = NODE_TYPE_EXPRESSION_PARENTHESES,
                    .exp.left = inner
                });

                node_push(paren_node);
                res = 0;
            }
            break;

        default:
            break;
    }

    // ✅ NOVO TRECHO PARA CONSUMIR ',' E ';'
    token = token_peek_next();
    if (token && token->type == TOKEN_TYPE_SYMBOL && (token->cval == ',' || token->cval == ';')) {
        //printf("DEBUG: Consumindo símbolo '%c'\n", token->cval);
        token_next(); // consumir e ignorar
        res = 0;      // considerar sucesso
    }

    return res;
}



// Analisa uma sequência de tokens expressáveis.
void parse_expressionable(struct history* history) {
    while (parse_expressionable_single(history) == 0) {}
}

// Processa o próximo elemento do código-fonte.
int parse_next() {
    struct token* token = token_peek_next();
    if (!token) return -1;

    switch (token->type) {
        case TOKEN_TYPE_NUMBER:
        case TOKEN_TYPE_IDENTIFIER:
        case TOKEN_TYPE_STRING:
            parse_expressionable(history_begin(0));
            break;
        case TOKEN_TYPE_KEYWORD:
            parse_keyword_for_global();
            break;

        default:
            break;
    }

    return 0;
}

int parse(struct compile_process* process) {
    current_process = process;
    parser_last_token = NULL;

    node_set_vector(process->node_vec, process->node_tree_vec);
    vector_set_peek_pointer(process->token_vec, 0);

    // Enquanto houver tokens, processa como expressão/declaração
    while (token_peek_next()) {
        parse_expressionable(history_begin(0));
    }

    return PARSE_ALL_OK;
}

void parser_reorder_expression(struct node** node_out) {
    struct node* node = *node_out;

    if (!node || node->type != NODE_TYPE_EXPRESSION)
        return;

    if (!node->exp.left || !node->exp.right)
        return;

    // Reordena a direita primeiro (profundidade)
    if (node->exp.right->type == NODE_TYPE_EXPRESSION) {
        parser_reorder_expression(&node->exp.right);
    }

    // Se a direita ainda for uma expressão, comparar precedência
    if (node->exp.right->type == NODE_TYPE_EXPRESSION) {
        const char* op = node->exp.op;
        const char* right_op = node->exp.right->exp.op;

        if (parser_left_op_has_priority(op, right_op)) {
            parser_node_shift_children_left(node);

            // Reaplica a reordenação nos filhos alterados
            parser_reorder_expression(&node->exp.left);
            parser_reorder_expression(&node->exp.right);
        }
    }

    // Reordena a esquerda se for expressão
    if (node->exp.left->type == NODE_TYPE_EXPRESSION) {
        parser_reorder_expression(&node->exp.left);
    }

    *node_out = node;
}




static int parser_get_precedence_for_operator(const char* op, struct expressionable_op_precedence_group** group_out) {
    *group_out = NULL;
    for (int i = 0; i < TOTAL_OPERATOR_GROUPS; i++) {
        for (int j = 0; op_precedence[i].operators[j]; j++) {
            const char* _op = op_precedence[i].operators[j];
            if (S_EQ(op, _op)) {
                *group_out = &op_precedence[i];
                return i;
            }
        }
    }
    return -1;
}

static bool parser_left_op_has_priority(const char* op_left, const char* op_right) {
    if (!op_left || !op_right) {
        fprintf(stderr, "[ERRO] Operador nulo detectado em parser_left_op_has_priority.\n");
        exit(1);
    }

    struct expressionable_op_precedence_group* group_left = NULL;
    struct expressionable_op_precedence_group* group_right = NULL;

    if (S_EQ(op_left, op_right)) return true;

    int precedence_left = parser_get_precedence_for_operator(op_left, &group_left);
    int precedence_right = parser_get_precedence_for_operator(op_right, &group_right);

    if (precedence_left == -1 || precedence_right == -1) return false;

    // Associatividade da direita para esquerda? Não dá shift
    if (group_right->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT)
        return false;

    return precedence_left <= precedence_right;
}



void parser_node_shift_children_left(struct node* node) {
    assert(node->type == NODE_TYPE_EXPRESSION);
    assert(node->exp.right && node->exp.right->type == NODE_TYPE_EXPRESSION);

    const char* right_op = node->exp.right->exp.op;

    struct node* new_exp_left_node = node->exp.left;
    struct node* new_exp_right_node = node->exp.right->exp.left;

    // Cria um novo nó de expressão com os filhos ajustados
    make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op);

    // O resultado dessa nova expressão é o novo filho da esquerda
    struct node* new_left_operand = node_pop();
    struct node* new_right_operand = node->exp.right->exp.right;

    node->exp.left = new_left_operand;
    node->exp.right = new_right_operand;
    node->exp.op = right_op;
}
