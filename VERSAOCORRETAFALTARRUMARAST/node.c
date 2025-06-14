#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>
#include <stdio.h>
void print_node_tree(struct vector* node_tree_vec, int indent) {
    for (int i = 0; i < vector_count(node_tree_vec); i++) {
        struct node* n = *(struct node**)vector_at(node_tree_vec, i);

        // Indentação
        for (int j = 0; j < indent; j++) printf("  ");

        // Impressão por tipo
        switch (n->type) {
            case NODE_TYPE_EXPRESSION:
                printf("Node type: %d, op: %s\n", n->type, n->exp.op);
                if (n->exp.left) print_node_tree_single(n->exp.left, indent + 1);
                if (n->exp.right) print_node_tree_single(n->exp.right, indent + 1);
                break;

            case NODE_TYPE_VARIABLE:
                printf("Node type: %d (VARIABLE: name=%s)\n", n->type, n->var.name ? n->var.name : "NULL");
                if (n->var.val) print_node_tree_single(n->var.val, indent + 1);
                break;

            case NODE_TYPE_VARIABLE_LIST:
                printf("Node type: %d (VARIABLE_LIST)\n", n->type);
                for (int j = 0; j < vector_count(n->var_list.list); j++) {
                    struct node* var = *(struct node**)vector_at(n->var_list.list, j);
                    print_node_tree_single(var, indent + 1);
                }
                break;

            default:
                printf("Node type: %d\n", n->type);
                break;
        }
    }
}



void print_node_tree_single(struct node* n, int indent) {
    if (!n) return;

    for (int j = 0; j < indent; j++) printf("  ");

    switch (n->type) {
        case NODE_TYPE_EXPRESSION:
            printf("EXPRESSION: op=%s\n", n->exp.op);
            print_node_tree_single(n->exp.left, indent + 1);
            print_node_tree_single(n->exp.right, indent + 1);
            break;

        case NODE_TYPE_VARIABLE:
            printf("VARIABLE: name=%s\n", n->var.name ? n->var.name : "NULL");
            if (n->var.val) {
                for (int j = 0; j < indent + 1; j++) printf("  ");
                printf("value:\n");
                print_node_tree_single(n->var.val, indent + 2);
            }
            break;

        case NODE_TYPE_NUMBER:
            printf("NUMBER: %u\n", n->inum);
            break;

        case NODE_TYPE_IDENTIFIER:
            printf("IDENTIFIER: %s\n", n->sval);
            break;

        case NODE_TYPE_STRING:
            printf("STRING: \"%s\"\n", n->sval);
            break;

        default:
            printf("Node type: %d\n", n->type);
            break;
    }
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void print_node(struct node* node, int indent) {
    if (!node) return;

    print_indent(indent);
    switch (node->type) {
        case NODE_TYPE_NUMBER:
            printf("NUMBER: %u\n", node->inum);
            break;
        case NODE_TYPE_VARIABLE:
            printf("VARIABLE: name=%s", node->var.name ? node->var.name : "NULL");
            //if (node->var.type) printf(", has type");
            printf(", has type");
            if (node->var.val) {
                printf(", value:\n");
                print_node(node->var.val, indent + 1);
            } else {
                printf("\n");
            }
            break;
        case NODE_TYPE_IDENTIFIER:
            printf("IDENTIFIER: %s\n", node->sval);
            break;
        case NODE_TYPE_STRING:
            printf("STRING: \"%s\"\n", node->sval);
            break;
        case NODE_TYPE_EXPRESSION:
            printf("EXPRESSION: %s\n", node->exp.op);
            print_node(node->exp.left, indent + 1);
            print_node(node->exp.right, indent + 1);
            break;
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            printf("PARENTHESES:\n");
            print_node(node->exp.left, indent + 1);
            break;
        case NODE_TYPE_UNARY:
            printf("UNARY: %s\n", node->exp.op);
            print_node(node->exp.left, indent + 1);
            break;
        case NODE_TYPE_TENARY:
            printf("TENARY:\n");
            print_node(node->exp.left, indent + 1);
            print_node(node->exp.right, indent + 1);
            break;
        default:
            printf("NODE_TYPE_%d (sem handler de print)\n", node->type);
            break;
    }
}


struct vector* node_vector = NULL;
struct vector* node_vector_root = NULL;

// Inicializa os ponteiros globais desse segmento de código.
void node_set_vector(struct vector* vec, struct vector* root_vec) {
    node_vector = vec;
    node_vector_root = root_vec;
}

// Adiciona o node ao final da lista.
void node_push(struct node* node) {
    vector_push(node_vector, &node);
}

// Pega o node da parte final do vetor. Se não tiver nada, retorna NULL.
struct node* node_peek_or_null() {
    return vector_back_ptr_or_null(node_vector);
}

struct node* node_peek() {
    return *(struct node**) vector_back(node_vector);
}

struct node* node_pop() {
    struct node* last_node = vector_back_ptr(node_vector);
    struct node* last_node_root = vector_empty(node_vector) ? NULL : vector_back_ptr_or_null(node_vector_root);

    vector_pop(node_vector);

    if (last_node == last_node_root)
        vector_pop(node_vector_root);

    return last_node;
}

// Tipos de nós que podem ser colocados dentro de uma expressão.
bool node_is_expressionable(struct node* node) {
    return node->type == NODE_TYPE_EXPRESSION ||
           node->type == NODE_TYPE_EXPRESSION_PARENTHESES ||
           node->type == NODE_TYPE_UNARY ||
           node->type == NODE_TYPE_IDENTIFIER ||
           node->type == NODE_TYPE_NUMBER ||
           node->type == NODE_TYPE_STRING;
}

// Verifica se o node informado pode ser colocado dentro de uma expressão.
struct node* node_peek_expressionable_or_null() {
    struct node* last_node = node_peek_or_null();
    return node_is_expressionable(last_node) ? last_node : NULL;
}

// Cria um node de expressão.
void make_exp_node(struct node* node_left, struct node* node_right, const char* op) {
    assert(node_left);
    assert(node_right);
    node_create(&(struct node){
        .type = NODE_TYPE_EXPRESSION,
        .exp.left = node_left,
        .exp.right = node_right,
        .exp.op = op
    });
}

// Função para criar um node.
struct node* node_create(struct node* _node) {
    struct node* node = malloc(sizeof(struct node));
    memcpy(node, _node, sizeof(struct node));
    node_push(node);
    return node;
}
