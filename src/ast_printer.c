#include "ast_printer.h"
#include <stdio.h>
#include "error.h"

static expr_elem* print_expr_elem(cunit* cu, expr_elem* e);
void reverse_print_func_args(cunit* cu, expr_elem* elem, expr_elem* end){
    if(elem == end)return;
    expr_elem* nxt;
    if( elem->id.type== EXPR_ELEM_TYPE_NUMBER ||
        elem->id.type== EXPR_ELEM_TYPE_VARIABLE ||
        elem->id.type== EXPR_ELEM_TYPE_LITERAL ||
        elem->id.type== EXPR_ELEM_TYPE_BINARY_LITERAL)
    {
        nxt = elem-2;
    }
    else{
         elem -= elem->id.nest_size;
    }
    if(nxt != end){
        reverse_print_func_args(cu, nxt, end);
        putchar(',');
        putchar(' ');
    }
    print_expr_elem(cu, elem);
}
void print_literal(cunit* cu, char* str){
    putchar('\"');
    printf(str);
    putchar('\"');
}
void print_number(cunit* cu, char* str){
    printf(str);
}
void print_binary_literal(cunit* cu, char* str){
    putchar('\'');
    printf(str);
    putchar('\'');
}
void print_op(u8 op){
	switch(op){
        case OP_ADD:putchar('+');return;
        case OP_PREINCREMENT:
        case OP_POSTINCREMENT: putchar('+');putchar('+'); return;
        case OP_ADD_ASSIGN: putchar('+');putchar('='); return;
        case OP_SUBTRACT:putchar('-'); return;
        case OP_PREDECREMENT:
        case OP_POSTDECREMENT: putchar('-');putchar('-'); return;
        case OP_SUBTRACT_ASSIGN:putchar('-');putchar('='); return;
        case OP_LOGICAL_NOT: putchar('!');return;
        case OP_NOT_EQUAL: putchar('!');putchar('='); return;
        case OP_MULTIPLY:
        case OP_DEREFERENCE: putchar('*'); return;
        case OP_MULTIPLY_ASSIGN:putchar('*');putchar('='); return;
        case OP_ASSIGN:putchar('='); return;
        case OP_EQUALS:putchar('=');putchar('='); return;
        case OP_MODULO: putchar('%'); return;
        case OP_MODULO_ASSIGN: putchar('%');putchar('='); return;
        case OP_DIVIDE: putchar('/'); return;
        case OP_DIVIDE_ASSIGN: putchar('/');putchar('='); return;
        case OP_BITWISE_AND: putchar('&'); return;
        case OP_LOGICAL_AND: putchar('&');putchar('&'); return;
        case OP_BITWISE_AND_ASSIGN: putchar('&');putchar('='); return;
        case OP_BITWISE_OR: putchar('|'); return;
        case OP_LOGICAL_OR: putchar('|');putchar('|'); return;
        case OP_BITWISE_OR_ASSIGN: putchar('|');putchar('='); return;
        case OP_BITWISE_NOT: putchar('~'); return;
        case OP_BITWISE_NOT_ASSIGN: putchar('~');putchar('='); return;
        case OP_LESS_THAN: putchar('<'); return;
        case OP_LSHIFT: putchar('<');putchar('<'); return;
        case OP_GREATER_THAN: putchar('>'); return;
        case OP_RSHIFT: putchar('>');putchar('>'); return;
        case OP_ACCESS_MEMBER: putchar('.'); return;
        case OP_DEREFERENCE_ACCESS_MEMBER: putchar('-');putchar('>'); return;
        case OP_LSHIFT_ASSIGN: putchar('<');putchar('<');putchar('=');return;
        case OP_RSHIFT_ASSIGN: putchar('>');putchar('>');putchar('=');return;
        case OP_BITWISE_XOR: putchar('^');return;
        case OP_BITWISE_XOR_ASSIGN: putchar('^');putchar('=');return;
        default:CIM_ERROR("Unknown token");
	}
}
static expr_elem* print_expr_elem(cunit* cu, expr_elem* e){
    switch(e->id.type){
        case EXPR_ELEM_TYPE_NUMBER:
        case EXPR_ELEM_TYPE_VARIABLE:
            printf((e-1)->str);
            break;
        case EXPR_ELEM_TYPE_LITERAL:
            print_literal(cu, (e-1)->str);
            break;
        case EXPR_ELEM_TYPE_BINARY_LITERAL:
            print_binary_literal(cu, (e-1)->str);
            break;
        case EXPR_ELEM_TYPE_OP_LR:{
            putchar('(');
            expr_elem* r = (void*)(e-1);
            expr_elem* l;
            if( r->id.type == EXPR_ELEM_TYPE_NUMBER ||
                r->id.type == EXPR_ELEM_TYPE_VARIABLE ||
                r->id.type == EXPR_ELEM_TYPE_LITERAL ||
                r->id.type == EXPR_ELEM_TYPE_BINARY_LITERAL)
            {
                l = r-2;
            }
            else {
                l= r - r->id.nest_size;
            }

            expr_elem* end_op = print_expr_elem(cu, l);
            putchar(' ');
            print_op(e->id.op);
            putchar(' ');
            print_expr_elem(cu, r);
            putchar(')');
            return end_op;
        }
        case EXPR_ELEM_TYPE_EXPR:{
            return print_expr_elem(cu, e-1);
        }
        case EXPR_ELEM_TYPE_OP_R:{
            putchar('(');
            expr_elem *u = e - 1;
            expr_elem* end_op = print_expr_elem(cu, u);
            print_op(e->id.op);
            putchar(')');
            return end_op;
        }
        case EXPR_ELEM_TYPE_OP_L:
        case EXPR_ELEM_TYPE_UNARY: {
            putchar('(');
            expr_elem *u = e - 1;
            print_op(e->id.op);
            expr_elem* end_op = print_expr_elem(cu, u);
            putchar(')');
            return end_op;
        }
        case EXPR_ELEM_TYPE_FN_CALL:{
            expr_elem* end = e - e->id.nest_size;
            e--;
            printf(e->str);
            e--;
            putchar('(');
            reverse_print_func_args(cu, e, end);
            putchar(')');
            return end;
        }
        case EXPR_ELEM_TYPE_GENERIC_FN_CALL:{
            expr_elem* end = e - e->id.nest_size;
            e--;
            printf(e->str);
            e--;
            expr_elem* generic_args_rstart = (void*)(cu->ast.start  + e->ast_pos);
            e--;
            putchar('[');
            reverse_print_func_args(cu, generic_args_rstart, end);
            putchar(']');putchar('(');
            reverse_print_func_args(cu, e, generic_args_rstart);
            putchar(')');
            return end;
        }
        case EXPR_ELEM_TYPE_ARRAY_ACCESS:{
            e--;
            printf(e->str);
            e--;
            putchar('[');
            e = print_expr_elem(cu, e);
            putchar(']');
            return e;
        }
        default:CIM_ERROR("Unknown expression type");
    }
    return e-2;
}
static inline expr_elem* print_expr(cunit* cu, expr_elem* expr){
    expr_elem* e = expr + expr->id.nest_size - 1;
    print_expr_elem(cu, e);
    return e+1;
}

void print_indent(ureg indent){
    for(ureg i=0;i<indent; i++)fputs("    ", stdout);
}
void print_ptrs(u8 ptrs){
    for(u8 i = 0;i!=ptrs;i++){
        putchar('*');
    }
}
ast_type_node* print_type(cunit* cu, ast_type_node* t);
void reverse_print_type_list(cunit* cu, ast_type_node* start, ast_type_node* end){
    ast_type_node* next = start - start->type.end;
    if(next != end){
        reverse_print_type_list(cu, next, end);
        putchar(',');putchar(' ');
    }
    print_type(cu, start);
}
ast_type_node* print_type(cunit* cu, ast_type_node* t){
    if(t->type.type == AST_TYPE_TYPE_SIMPLE){
        fputs((t-1)->str, stdout);
        print_ptrs(t->type.ptrs);
        return t + 1;
    }
    ast_type_node* last = t - t->type.end + 1;
    ast_type_node* tn = t-1;
    switch (t->type.type){
        case AST_TYPE_TYPE_SCOPED:{
            for(ast_type_node* i = last ; i != tn; i++){
                fputs(i->str, stdout);
                putchar(':');
            }
            fputs(tn->str, stdout);
        }break;
        case AST_TYPE_TYPE_GENERIC_STRUCT:{
            fputs(tn->str, stdout);
            putchar('[');
            reverse_print_type_list(cu, t - 2, last-1);
            putchar(']');
        }break;
        case AST_TYPE_TYPE_SCOPED_GENERIC_STRUCT:{
            ast_type_node* scopes_end = last + tn->type.end;
            for(ast_type_node* i = last; i!= scopes_end; i++){
                fputs(i->str, stdout);
                putchar(':');
            }
            fputs((t-2)->str, stdout);
            putchar('[');
            reverse_print_type_list(cu, t - 3, scopes_end - 1);
            putchar(']');
        }break;
        case AST_TYPE_TYPE_FN_PTR:{
            ast_type_node* ret = last + tn->type.end-1;
            putchar('(');
            print_type(cu, ret);
             putchar(' ');putchar('(');
            reverse_print_type_list(cu, (t-2), ret);
            putchar(')');
            putchar(')');
        }break;
        default:CIM_ERROR("Unknown AST_TYPE_TYPE");
    }
    print_ptrs(t->type.ptrs);
    return t + 1;
}
void print_ast(cunit* cu){
    u8* astn = (void*)cu->ast.start;
    u8* end = (void*)cu->ast.head;
    ureg indent = 0;
    while(astn!=end){
        switch(*astn){
            case ASTNT_DECLARATION:{
                print_indent(indent);
                astn_declaration* d = (void*)astn;
                astn+= sizeof(*d);
                printf(d->type);
                putchar(' ');
                printf(d->name);
                if(d->assigning == false){
                    putchar(';');
                    putchar('\n');
                    break;
                }
                fputs(" = ", stdout);
            }break;
            case ASTNT_EXPRESSION:{
                astn = (void*)print_expr(cu, (void*)astn);
                puts(";");
            }break;
            case ASTNT_VARIABLE:
            case ASTNT_NUMBER:{
                expr_elem* n = (void*)astn;
                astn+= sizeof(*n) * 2;
                printf((n+1)->str);
                putchar(';');
                putchar('\n');
            }break;
            case ASTNT_LITERAL:{
                expr_elem* e = (void*)astn;
                print_literal(cu, (e+1)->str);
                putchar(';');
                putchar('\n');
            }break;
            case ASTNT_BINARY_LITERAL:{
                expr_elem* e = (void*)astn;
                print_binary_literal(cu, (e+1)->str);
                putchar(';');
                putchar('\n');
            }break;
            case ASTNT_TYPEDEF:{
                astn_typedef* t = (void*)astn;
                printf("typedef %s ", t->tgt_type.str);
                ast_type_node* tn = (ast_type_node*)(t+1) + t->end;
                astn = (void*)print_type(cu, tn);
                putchar(';');
                putchar('\n');
            }break;
            default:CIM_ERROR("Unexpected ASTN");
        }
    }
}