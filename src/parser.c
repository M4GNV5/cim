#include "parser.h"
#include "tokenizer.h"
#include <stdio.h>
#include <memory.h>
#include "ast.h"
#define POS(x)(((u8*)(x)) - cu->ast.start)
#define UNEXPECTED_TOKEN() do{                                         \
    printf("%s(%d): Unexpected token", __FILE__, __LINE__); exit(-1); \
}while(0)
#define OP_RANGE 255
static u8 prec_table [OP_RANGE];
static u8 assoc_table [OP_RANGE];
enum assocs{
    RIGHT_ASSOCIATIVE = 0,
    LEFT_ASSOCIATIVE = 1,   //this cant be zero or our initialization flag won't work
};
void init(cunit* cu){
    if(prec_table[0] == 0){
        prec_table[0] = 1; //initialization flag
        prec_table['('] = 0;   // '(' uniquely has this precedence level so it is not removed during flush
        prec_table['+'] = 1;
        prec_table['-'] = 1;
        prec_table ['%'] = 2;
        prec_table['/'] = 3;
        prec_table[OPS_UNYRY_PLUS] = 99;
        prec_table[OPS_UNARY_MINUS] = 99;
    }
    if(assoc_table[0] == 0){
        memset(assoc_table, LEFT_ASSOCIATIVE, OP_RANGE);
    }
	dbuffer_init(&cu->string_store);
	dbuffer_init(&cu->string_ptrs);
	dbuffer_init(&cu->ast);
    dbuffer_init(&cu->shy_ops);
}

//we do the mask because a cast to an int of lower width is implementation defined for values
//outside of its range. this will be optimized away anyway


void print_indent(ureg indent){
    for(ureg i=0;i<indent; i++)fputs("    ", stdout);
}
static void print_expr_elem(cunit* cu, expr_elem* e){
    switch(e->type){
        case EXPR_ELEM_TYPE_NUMBER:
            print_rel_str(cu, e->val.number_str);
            break;
        case EXPR_ELEM_TYPE_OP_LR:{
            putchar('(');
            expr_elem* r = (void*)(e-1);
            expr_elem* l;
            if(r->type == EXPR_ELEM_TYPE_NUMBER){
                l = r-1;
            }
            else{
                l = (void*)(cu->ast.start + r->val.op_rend);
            }
            print_expr_elem(cu, l);
            putchar(' ');
            token t;
            t.type = TOKEN_TYPE_OPERATOR_LR;
            t.str = (ureg)e->op;
            print_token(cu, &t);
            putchar(' ');
            print_expr_elem(cu, r);
            putchar(')');
        }break;
        case EXPR_ELEM_TYPE_UNARY: {
            putchar('(');
            expr_elem *u = (void *) (e - 1);
            token t;
            t.type = TOKEN_TYPE_POSSIBLY_UNARY;
            t.str = (ureg) e->op;
            print_token(cu, &t);
            print_expr_elem(cu, u);
            putchar(')');
        }break;
        default: printf("Unknown expr type"); exit(-1);
    }
}
static inline u8* print_expr(cunit* cu, astn_expression* expr){
    expr_elem* e = (void*) cu->ast.start + expr->end - sizeof(expr_elem);
    print_expr_elem(cu, e);
    return cu->ast.start + expr->end;
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
                print_rel_str(cu, d->type);
                putchar(' ');
                print_rel_str(cu, d->name);
                if(d->assigning == false){
                    putchar(';');
                    putchar('\n');
                    break;
                }
                fputs(" = ", stdout);
            }break;
            case ASTNT_EXPRESSION:{
                astn = print_expr(cu, (void*)astn);
                puts(";");
            }break;
            case ASTNT_NUMBER:{
                astn_number* n = (void*)astn;
                astn+= sizeof(*n);
                print_rel_str(cu, n->number_str);
                putchar(';');
                putchar('\n');
            }break;
            case ASTNT_VARIABLE:{
                astn_variable* v = (void*)astn;
                astn+= sizeof(*v);
                print_rel_str(cu, v->name);
                putchar(';');
                putchar('\n');
            }break;
            default:{
                 UNEXPECTED_TOKEN();
            }return;
        } 
    }
}
typedef struct shy_op_t{
    u8 type;
    u8 op;
}shy_op;
static void parse_meta(cunit* cu, token* t1){
    
}
static inline void flush_shy_op(cunit* cu, shy_op* s, ureg expr_start){
    expr_elem* expr_rend = (void*)(cu->ast.start + expr_start + sizeof(astn_expression) - sizeof(expr_elem));
    expr_elem* expr_rit =  (void*)(cu->ast.head - sizeof(expr_elem));
    int its;
    switch (s->type){
        case EXPR_ELEM_TYPE_BRACE: its = 0;break;
        case EXPR_ELEM_TYPE_OP_LR: its = 2;break;
        default: its = 1; break;
    }
    for(int i = 0; i != its; i++){
        if(expr_rit->type == EXPR_ELEM_TYPE_NUMBER){
            expr_rit--;
        }
        else{
            expr_rit = (void*)cu->ast.start + expr_rit->val.op_rend;
        }
    }
    expr_elem* e = dbuffer_claim_small_space(&cu->ast, sizeof(expr_elem));
    e->type = s->type;
    e->op = s->op;
    e->val.op_rend = (u8*)expr_rit - cu->ast.start;
    cu->shy_ops.head -= sizeof(shy_op); //this will be inlined and brought out of the loop
}
static inline void push_shy_op(cunit* cu, shy_op* sop, shy_op** sho_ri, shy_op** sho_re, ureg shy_ops_start){
    if(dbuffer_has_space(&cu->shy_ops, sizeof(shy_op))){
        *((shy_op*)cu->shy_ops.head) = *sop;
        cu->shy_ops.head += sizeof(shy_op);
        (*sho_ri)++;
    }
    else{
        dbuffer_grow(&cu->shy_ops);
        *((shy_op*)cu->shy_ops.head) = *sop;
        cu->shy_ops.head += sizeof(shy_op);
        *sho_re = (void*)(cu->shy_ops.start + shy_ops_start - sizeof(shy_op));
        *sho_ri = (void*)(cu->shy_ops.head - sizeof(shy_op));
    }
}
static int parse_expr(cunit *cu, char term, token *t1, token *t2){
    //printf("parsing expr (%llu)\n", POS(cu->ast.head));
    if(t2->type == term){
        //short expression optimization
        if(t1->type == TOKEN_TYPE_NUMBER){
            astn_number* n = dbuffer_claim_small_space(&cu->ast, sizeof(astn_number));
            n->astnt = ASTNT_NUMBER;
            n->number_str = t1->str;
        }
		if(t1->type == TOKEN_TYPE_STRING){
            astn_variable* v = dbuffer_claim_small_space(&cu->ast, sizeof(astn_variable));
            v->astnt = ASTNT_VARIABLE;
            v->name = t1->str;
        }
        return 0;
    }
    astn_expression* expr = dbuffer_claim_small_space(&cu->ast, sizeof(astn_expression));
    expr->astnt = ASTNT_EXPRESSION;
    ureg expr_start = (u8*)expr - cu->ast.start;
    ureg expr_elems_start = expr_start + sizeof(astn_expression);
    expr_elem* e;
    shy_op sop;
    bool handled_first = false;
    ureg shy_ops_start = cu->shy_ops.head - cu->shy_ops.start;
    u8 last_prec = 0;
    bool expecting_op = false;
    u8 prec;
    shy_op* sho_re = (void*)(cu->shy_ops.start + shy_ops_start - sizeof(shy_op));
    shy_op* sho_ri = (void*)(cu->shy_ops.head - sizeof(shy_op));
    while(true){
        if(t1->type == term){
                for(;sho_ri != sho_re; sho_ri--){
                    flush_shy_op(cu, sho_ri, expr_start);
                }
                expr = (astn_expression*)(cu->ast.start + expr_start);
                expr->end = dbuffer_get_size(&cu->ast);
                return 0;
        }
        switch(t1->type){
            case TOKEN_TYPE_POSSIBLY_UNARY:{
                if(!expecting_op){
                    sop.type = EXPR_ELEM_TYPE_UNARY;
                    switch(TO_U8(t1->str)){
                        case '+': sop.op = OPS_UNYRY_PLUS;break;
                        case '-': sop.op = OPS_UNARY_MINUS;break;
                        default: printf("Unexpected unary op"); exit(-1);
                    }
                    push_shy_op(cu, &sop, &sho_ri, &sho_re, shy_ops_start);
                    last_prec = prec_table[sop.op];
                    //expecting_op stays on false
                    break;
                }
                //in this case we fall through to LR
            }
            case TOKEN_TYPE_OPERATOR_LR:{
                sop.type = EXPR_ELEM_TYPE_OP_LR;
                sop.op = TO_U8(t1->str);
                prec = prec_table[sop.op];
                if(last_prec >= prec){
                    if(assoc_table[sop.op] == LEFT_ASSOCIATIVE){
                        for(; sho_ri != sho_re && prec_table[sho_ri->op] >= prec;sho_ri--){
                            flush_shy_op(cu, sho_ri, expr_start);
                        }
                    }
                    else{
                        for(; sho_ri != sho_re && prec_table[sho_ri->op] > prec;sho_ri--){
                            flush_shy_op(cu, sho_ri, expr_start);
                        }
                    }
                }
                last_prec = prec;
                push_shy_op(cu, &sop, &sho_ri, &sho_re, shy_ops_start);
                expecting_op = false;
            }break;
            case TOKEN_TYPE_NUMBER:
                e = dbuffer_claim_small_space(&cu->ast, sizeof(*e));
                e->type = EXPR_ELEM_TYPE_NUMBER;
                e->val.number_str = t1->str;
                expecting_op = true;
                break;
            case '(': {
                sop.type = EXPR_ELEM_TYPE_BRACE;
                sop.op = '(';
                push_shy_op(cu, &sop, &sho_ri, &sho_re, shy_ops_start);
                expecting_op = false;
                //last_prec = 0;
            }break;
            case ')':{
                for(;sho_ri != sho_re && sho_ri->op != '('; sho_ri--){
                    flush_shy_op(cu, sho_ri, expr_start);
                }
                //removing the (
                dbuffer_pop_back(&cu->shy_ops, sizeof(shy_op));
                sho_ri--;
                expecting_op = true;
            }break;
            case TOKEN_TYPE_EOF:
                printf("Unexpected eof.");exit(-1);
        }
        if(handled_first){
            get_token(cu, t1);
        }
        else {
            handled_first = true;
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
    if(t1->type == ';'){
        d->assigning = false;
    }
    else if(t1->type == '='){
        d->assigning = true;
        get_token(cu, t1);
        get_token(cu, t2);
        return parse_expr(cu, ';', t1, t2);
    }
    else{
        UNEXPECTED_TOKEN();    
    }
    return 0;
    printf("parsed declaration\n"); 
}
static int parse_next(cunit* cu){
    token t1;
    token t2;
    get_token(cu, &t1);
    switch (t1.type){
        case TOKEN_TYPE_STRING:
            get_token(cu, &t2);
            if (t2.type == TOKEN_TYPE_STRING) 
                return parse_normal_declaration(cu, &t1, &t2);
            else return parse_expr(cu, ';', &t1, &t2);
        case TOKEN_TYPE_NUMBER:
            get_token(cu, &t2);
            return parse_expr(cu, ';', &t1, &t2);
        case '#':
        case TOKEN_TYPE_DOUBLE_HASH:
            parse_meta(cu, &t1);
            break;
        case TOKEN_TYPE_EOF:
            return -1;
        default:
            printf("parse_next: unexpected token\n");
            return -1;
    }
    return 0;
}
void parse(cunit* cu, char* str){
    cu->str = str;
    /*
    token t;
	get_token(cu, &t);
	while(t.type != TOKEN_TYPE_EOF){
		print_token(cu, &t);
		printf(" (%i)", t.rel_str);
		putchar('\n');
		get_token(cu, &t);
	}	
    */
    // actual parsing
    while(parse_next(cu)!=-1); 
}
