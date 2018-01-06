#include "parser.h"
#include "tokenizer.h"
#include <stdio.h>
#include <memory.h>
#include "ast.h"
#include <assert.h>
#include "error.h"
#include "sbuffer.h"

#define OP_RANGE (1 << (sizeof(u8)*8))
static u8 prec_table [OP_RANGE];
static u8 assoc_table [OP_RANGE];
enum assocs{
    LEFT_ASSOCIATIVE = 0,
    RIGHT_ASSOCIATIVE = 1,
};
void cunit_init(cunit* cu){
    if(prec_table[OP_NONE] == 0){
        prec_table[OP_NONE] = 1; //initialization flag
        //uniquely has this precedence level so it is not removed during flush
        prec_table[OP_TEMP_PAREN_OPEN] = 0;

        prec_table[OP_POSTINCREMENT]=15;
        prec_table[OP_POSTINCREMENT]=15;

        prec_table[OP_PREINCREMENT]=14;
        prec_table[OP_PREDECREMENT]=14;
        prec_table[OP_LOGICAL_NOT]=14;
        prec_table[OP_BITWISE_NOT]=14;
        prec_table[OP_DEREFERENCE]=14;
        prec_table[OP_ADDRESS_OF]=14;
        prec_table[OP_MULTIPLY]=13;
        prec_table[OP_DIVIDE]=13;
        prec_table[OP_MODULO]=13;

        prec_table[OP_ADD]=12;
        prec_table[OP_SUBTRACT]=12;

        prec_table[OP_LSHIFT]=11;
        prec_table[OP_RSHIFT]=11;

        prec_table[OP_LESS_THAN]=10;
        prec_table[OP_GREATER_THAN]=10;
        prec_table[OP_LESS_THAN_OR_EQUAL]=10;
        prec_table[OP_GREATER_THAN_OR_EQUAL]=10;

        prec_table[OP_EQUALS] = 9;
        prec_table[OP_NOT_EQUAL] = 9;

        prec_table[OP_BITWISE_AND] = 8;

        prec_table[OP_BITWISE_XOR] = 7;

        prec_table[OP_BITWISE_OR] = 6;

        prec_table[OP_LOGICAL_AND] = 5;

        prec_table[OP_LOGICAL_XOR] = 4;

        prec_table[OP_LOGICAL_OR] = 3;

        prec_table[OP_ASSIGN] = 2;
        prec_table[OP_ADD_ASSIGN] = 2;
        prec_table[OP_SUBTRACT_ASSIGN] = 2;
        prec_table[OP_MULTIPLY_ASSIGN] = 2;
        prec_table[OP_DIVIDE_ASSIGN] = 2;
        prec_table[OP_MODULO_ASSIGN] = 2;
        prec_table[OP_LSHIFT_ASSIGN] = 2;
        prec_table[OP_RSHIFT_ASSIGN] = 2;
        prec_table[OP_BITWISE_AND_ASSIGN] = 2;
        prec_table[OP_BITWISE_OR_ASSIGN] = 2;
        prec_table[OP_BITWISE_XOR_ASSIGN] = 2;
        prec_table[OP_BITWISE_NOT_ASSIGN] = 2;
    }
    if(assoc_table[OP_NONE] == LEFT_ASSOCIATIVE){
        assoc_table[OP_NONE] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_PREINCREMENT]=RIGHT_ASSOCIATIVE;
        assoc_table[OP_PREDECREMENT]=RIGHT_ASSOCIATIVE;
        assoc_table[OP_LOGICAL_NOT]=RIGHT_ASSOCIATIVE;
        assoc_table[OP_BITWISE_NOT]=RIGHT_ASSOCIATIVE;
        assoc_table[OP_DEREFERENCE]=RIGHT_ASSOCIATIVE;
        assoc_table[OP_ADDRESS_OF]=RIGHT_ASSOCIATIVE;
        assoc_table[OP_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_ADD_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_SUBTRACT_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_MULTIPLY_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_DIVIDE_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_MODULO_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_LSHIFT_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_RSHIFT_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_BITWISE_AND_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_BITWISE_OR_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_BITWISE_XOR_ASSIGN] = RIGHT_ASSOCIATIVE;
        assoc_table[OP_BITWISE_NOT_ASSIGN] = RIGHT_ASSOCIATIVE;
    }
	sbuffer_init(&cu->data_store, 4);
	dbuffer_init(&cu->string_ptrs);
	dbuffer_init(&cu->ast);
    dbuffer_init(&cu->shy_ops);
}
void cunit_fin(cunit* cu){
   	sbuffer_fin(&cu->data_store);
	dbuffer_fin(&cu->string_ptrs);
	dbuffer_fin(&cu->ast);
    dbuffer_fin(&cu->shy_ops);
}

static void parse_meta(cunit* cu, token* t1){
    
}
static inline void flush_shy_op(cunit* cu, expr_elem* s){
    expr_elem* expr_rit =  (void*)(cu->ast.head - sizeof(expr_elem));
    int its;
    switch (s->id.type){
        case EXPR_ELEM_TYPE_PAREN: its = 0;break;
        case EXPR_ELEM_TYPE_OP_LR: its = 2;break;
        default: its = 1; break;
    }
    for(int i = 0; i != its; i++){
        if( expr_rit->id.type== EXPR_ELEM_TYPE_NUMBER ||
            expr_rit->id.type== EXPR_ELEM_TYPE_VARIABLE ||
            expr_rit->id.type== EXPR_ELEM_TYPE_LITERAL ||
            expr_rit->id.type== EXPR_ELEM_TYPE_BINARY_LITERAL)
        {
            expr_rit-=2;
        }
        else{
            expr_rit--;
            expr_rit = (void*)cu->ast.start + expr_rit->ast_pos;
        }
    }
    expr_elem* e = dbuffer_claim_small_space(&cu->ast, sizeof(expr_elem)*2);
    //TODO: evaluate the necessity of this for single arg ops
    e->ast_pos= (u8*)expr_rit - cu->ast.start;
    e++;
    *e = *s;
    //this will hopefully be inlined and brought out of the loop
    cu->shy_ops.head -= sizeof(expr_elem);
}
static inline void push_shy_op(cunit* cu, expr_elem* sop, expr_elem** sho_ri, expr_elem** sho_re, ureg shy_ops_start){
    if(dbuffer_has_space(&cu->shy_ops, sizeof(expr_elem))){
        *((expr_elem*)cu->shy_ops.head) = *sop;
        cu->shy_ops.head += sizeof(expr_elem);
        (*sho_ri)++;
    }
    else{
        dbuffer_grow(&cu->shy_ops);
        *((expr_elem*)cu->shy_ops.head) = *sop;
        cu->shy_ops.head += sizeof(expr_elem);
        *sho_re = (void*)(cu->shy_ops.start + shy_ops_start - sizeof(expr_elem));
        *sho_ri = (void*)(cu->shy_ops.head - sizeof(expr_elem));
    }
}
static int parse_expr(cunit *cu, token_type term1, token_type term2, token *t1, token *t2, bool sub_expr);
static inline ureg parse_arg_list(cunit* cu, token* t1, token* t2, token_type end_tok){
    get_token(cu, t1);
    if(t1->type == end_tok)return 0;
    get_token(cu, t2);
    ureg its = 1;
    while (parse_expr(cu, TOKEN_COMMA, end_tok, t1, t2, true) == 0){
        its++;
        get_token(cu, t1);
        get_token(cu, t2);
    }
    return its;
}
static int parse_expr(cunit *cu, token_type term1, token_type term2, token *t1, token *t2, bool sub_expr){
    //printf("parsing expr (%llu)\n", POS(cu->ast.head));
    if(t2->type == term1 || t2->type == term2){
        //short expression optimization
        if( t1->type == TOKEN_NUMBER ||
            t1->type == TOKEN_BINARY_LITERAL ||
            t1->type == TOKEN_LITERAL)
        {
            expr_elem* n = dbuffer_claim_small_space(&cu->ast, sizeof(expr_elem) * 2);
            if(!sub_expr){
                n->id.type = (u8)t1->type; //token type matches expr elem type for these
                n++;
                n->str = t1->str;
            }
            else{

            }

        }
        else if(t1->type == TOKEN_STRING) {
            expr_elem* v = dbuffer_claim_small_space(&cu->ast, sizeof(expr_elem) * 2);
            v->id.type= ASTNT_VARIABLE;
            v++;
            v->str = t1->str;
        }
        else{
            CIM_ERROR("Unexpected Token");
        }
        return (t2->type == term1) ? 0 : 1;
    }
    ureg expr_start;
    expr_elem* expr;
    if(!sub_expr){
        //second one is for expression size
        expr = dbuffer_claim_small_space(&cu->ast, sizeof(expr_elem) * 2);
        expr->id.type= EXPR_ELEM_TYPE_EXPR;
        expr_start = (u8*)expr - cu->ast.start;
    }
    else{
        expr_start = dbuffer_get_size(&cu->ast) - sizeof(expr_elem);
    }
    expr_elem* e;
    expr_elem sop;
    bool second_available = true;
    ureg shy_ops_start = cu->shy_ops.head - cu->shy_ops.start;
    bool expecting_op = false;
    u8 prec;
    expr_elem* sho_re = (void*)(cu->shy_ops.start + shy_ops_start - sizeof(expr_elem));
    expr_elem* sho_ri = (void*)(cu->shy_ops.head - sizeof(expr_elem));
    ureg open_paren_count = 0;
    while(true){
        switch(t1->type){
            case TOKEN_DOUBLE_PLUS: {
                sop.id.op = (expecting_op) ? OP_POSTINCREMENT : OP_PREINCREMENT;
            }goto lbl_op_l_or_r;
            case TOKEN_DOUBLE_MINUS:{
                sop.id.op = (expecting_op) ? OP_POSTDECREMENT : OP_PREDECREMENT;
            }//fallthrough to op_l_or_r
            lbl_op_l_or_r:{
                sop.id.type = (expecting_op) ? EXPR_ELEM_TYPE_OP_R : EXPR_ELEM_TYPE_OP_L;
                prec = prec_table[sop.id.op];
                if (assoc_table[sop.id.op] == LEFT_ASSOCIATIVE) {
                    for (; sho_ri != sho_re && prec_table[sho_ri->id.op] >= prec; sho_ri--) {
                        flush_shy_op(cu, sho_ri);
                    }
                }
                else {
                    for (; sho_ri != sho_re && prec_table[sho_ri->id.op] > prec; sho_ri--) {
                        flush_shy_op(cu, sho_ri);
                    }
                }
                push_shy_op(cu, &sop, &sho_ri, &sho_re, shy_ops_start);
                //expecting op stays the same
            }break;
            case TOKEN_STAR:{
                if (expecting_op) {
                    sop.id.op = OP_MULTIPLY;
                    goto lbl_op_lr;
                }
                sop.id.op = OP_DEREFERENCE;
            }goto lbl_op_unary;
            case TOKEN_AND:{
                if (expecting_op) {
                    sop.id.op = OP_BITWISE_AND;
                    goto lbl_op_lr;
                }
                sop.id.op = OP_ADDRESS_OF;
            }goto lbl_op_unary;
            case TOKEN_PLUS: {
                if (expecting_op) {
                    sop.id.op = OP_ADD;
                    goto lbl_op_lr;
                }
                sop.id.op = OP_UNARY_PLUS;
            }goto lbl_op_unary;
            case TOKEN_MINUS:{
                if (expecting_op) {
                    sop.id.op = OP_SUBTRACT;
                    goto lbl_op_lr;
                }
                sop.id.op = OP_UNARY_MINUS;
            }//fallthrough to lbl_op_unary
            lbl_op_unary: {
                sop.id.type = EXPR_ELEM_TYPE_UNARY;
                prec = prec_table[sop.id.op];
                //unary is always right associative
                for (; sho_ri != sho_re && prec_table[sho_ri->id.op] > prec; sho_ri--) {
                    flush_shy_op(cu, sho_ri);
                }
                push_shy_op(cu, &sop, &sho_ri, &sho_re, shy_ops_start);
                //expecting_op is already false, otherwise it wouldn't be unary
            } break;
            case TOKEN_SLASH:
            case TOKEN_SLASH_EQUALS:
            case TOKEN_STAR_EQUALS:
            case TOKEN_LESS_THAN:
            case TOKEN_LESS_THAN_EQUALS:
            case TOKEN_GREATER_THAN:
            case TOKEN_GREATER_THAN_EQUALS:
            case TOKEN_DOUBLE_LESS_THAN:
            case TOKEN_DOUBLE_GREATER_THAN:
            case TOKEN_EQUALS:
            case TOKEN_DOUBLE_EQUALS:
            case TOKEN_EXCLAMATION_MARK_EQUALS:
            case TOKEN_MINUS_EQUALS:
            case TOKEN_PLUS_EQUALS:
            case TOKEN_PERCENT:
            case TOKEN_PERCENT_EQUALS:
            case TOKEN_DOUBLE_GREATER_THAN_EQUALS:
            case TOKEN_DOUBLE_LESS_THAN_EQUALS:{
                //for these, the toke  type is set to be equal to the op type
                sop.id.op = (u8)(t1->type);
            }//fall through to op_lr
            lbl_op_lr: {
                sop.id.type = EXPR_ELEM_TYPE_OP_LR;
                prec = prec_table[sop.id.op];
                if (assoc_table[sop.id.op] == LEFT_ASSOCIATIVE) {
                    for (; sho_ri != sho_re && prec_table[sho_ri->id.op] >= prec; sho_ri--) {
                        flush_shy_op(cu, sho_ri);
                    }
                }
                else {
                    for (; sho_ri != sho_re && prec_table[sho_ri->id.op] > prec; sho_ri--) {
                        flush_shy_op(cu, sho_ri);
                    }
                }
                push_shy_op(cu, &sop, &sho_ri, &sho_re, shy_ops_start);
                expecting_op = false;
            }break;
            case TOKEN_PAREN_OPEN: {
                open_paren_count++;
                sop.id.type = EXPR_ELEM_TYPE_PAREN;
                sop.id.op = OP_TEMP_PAREN_OPEN;
                push_shy_op(cu, &sop, &sho_ri, &sho_re, shy_ops_start);
                expecting_op = false;
            }break;
            case TOKEN_PAREN_CLOSE: {
                if(open_paren_count==0)goto lbl_default;
                open_paren_count--;
                for (;sho_ri != sho_re && sho_ri->id.op != OP_TEMP_PAREN_OPEN;
                      sho_ri--)
                {
                    flush_shy_op(cu, sho_ri);
                }
                //removing the OP_TEMP_PAREN_OPEN
                dbuffer_pop_back(&cu->shy_ops, sizeof(expr_elem));
                sho_ri--;
                expecting_op = true;
            }break;
            case TOKEN_NUMBER:
            case TOKEN_LITERAL:
            case TOKEN_BINARY_LITERAL:
            {
                assert(!expecting_op);
                e = dbuffer_claim_small_space(&cu->ast, sizeof(*e) * 2);
                e->str = t1->str;
                e++;
                e->id.type= (u8)t1->type;
                expecting_op = true;
            }break;
            case TOKEN_STRING: {
                assert(!expecting_op);
                if (!second_available) {
                    get_token(cu, t2);
                }
                if (t2->type == TOKEN_PAREN_OPEN) {
                    char* fn_name = t1->str;
                    ureg fn_end = dbuffer_get_size(&cu->ast) - sizeof(expr_elem);
                    parse_arg_list(cu, t1, t2, TOKEN_PAREN_CLOSE);
                    e = dbuffer_claim_small_space(&cu->ast, sizeof(*e) * 3);
                    e->str = fn_name;
                    e++;
                    e->ast_pos= fn_end;
                    e++;
                    e->id.type= EXPR_ELEM_TYPE_FN_CALL;
                }
                else if(t2->type == TOKEN_BRACKET_OPEN){
                    char* el_name = t1->str;
                    ureg el_end = dbuffer_get_size(&cu->ast) - sizeof(expr_elem);
                    ureg its = parse_arg_list(cu, t1, t2, TOKEN_BRACKET_CLOSE);
                    get_token(cu, t2);
                    if(t2->type == TOKEN_PAREN_OPEN){
                        ureg generic_args_rstart = dbuffer_get_size(&cu->ast) - sizeof(*e) * 2;
                        parse_arg_list(cu, t1, t2, TOKEN_PAREN_CLOSE);
                        e = dbuffer_claim_small_space(&cu->ast, sizeof(*e) * 4);
                        e->ast_pos = generic_args_rstart;
                        e++;
                        e->str = el_name;
                        e++;
                        e->ast_pos= el_end;
                        e++;
                        e->id.type= EXPR_ELEM_TYPE_GENERIC_FN_CALL;
                    }
                    else{
                        e = dbuffer_claim_small_space(&cu->ast, sizeof(*e) * 3);
                        e->str= el_name;
                        e++;
                        e->ast_pos= el_end;
                        e++;
                        e->id.type= EXPR_ELEM_TYPE_ARRAY_ACCESS;
                        second_available=true;
                    }
                }
                else {
                    e = dbuffer_claim_small_space(&cu->ast, sizeof(*e) * 2);
                    e->str= t1->str;
                    e++;
                    e->id.type= EXPR_ELEM_TYPE_VARIABLE;
                    second_available=true;
                }
                //true for all: fn call, var, array access and generic fn call
                expecting_op = true;
            }break;
            case TOKEN_EOF:CIM_ERROR("Unexpected eof");
            default:{
lbl_default:
                if(t1->type == term1 || t1->type == term2){
                    for(;sho_ri != sho_re; sho_ri--){
                        flush_shy_op(cu, sho_ri);
                    }
                    if(!sub_expr){
                        //plus one to access the expr_end
                        expr = (expr_elem*)(cu->ast.start + expr_start) + 1;
                        expr->ast_pos= dbuffer_get_size(&cu->ast);
                    }
                    else{
                        expr = dbuffer_claim_small_space(&cu->ast, sizeof(expr_elem) * 2);
                        expr->ast_pos= expr_start;
                        expr++;
                        expr->id.type= ASTNT_EXPRESSION;
                    }
                    return (t1->type == term1) ? 0 : 1;
                }
                CIM_ERROR("Unexpected token");
            }
        }
        if(!second_available){
            get_token(cu, t1);
        }
        else {
            second_available = false;
            *t1 = *t2;
        }
    }
}
static int parse_normal_declaration(cunit* cu, token* t1, token* t2){
    astn_declaration* d =
     dbuffer_claim_small_space(&cu->ast, sizeof(astn_declaration));
    d->astnt = ASTNT_DECLARATION;
    d->type = t1->str;
    d->name = t2->str;
    get_token(cu, t1);
    if(t1->type == TOKEN_SEMICOLON){
        d->assigning = false;
    }
    else if(t1->type == TOKEN_EQUALS){
        d->assigning = true;
        get_token(cu, t1);
        get_token(cu, t2);
        return parse_expr(cu, TOKEN_SEMICOLON,TOKEN_SEMICOLON, t1, t2, false);
    }
    else{
       CIM_ERROR("Unexpected token");
    }
    return 0;
}
static int parse_next(cunit* cu){
    token t1;
    token t2;
    get_token(cu, &t1);
    switch (t1.type){
        case TOKEN_STRING:
            get_token(cu, &t2);
            if (t2.type == TOKEN_STRING)
                return parse_normal_declaration(cu, &t1, &t2);
            else return parse_expr(cu, TOKEN_SEMICOLON, TOKEN_SEMICOLON, &t1, &t2, false);
        case TOKEN_NUMBER:
            get_token(cu, &t2);
            return parse_expr(cu, TOKEN_SEMICOLON,TOKEN_SEMICOLON, &t1, &t2, false);
        case TOKEN_HASH:
        case TOKEN_DOUBLE_HASH:
            parse_meta(cu, &t1);
            break;
        case TOKEN_EOF:
            return -1;
        default:
            printf("parse_next: unexpected token\n");
            return -1;
    }
    return 0;
}
void parse(cunit* cu, char* str){
    cu->str = str;
    while(parse_next(cu)!=-1); 
}
