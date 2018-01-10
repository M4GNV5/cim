#pragma once
#include "compiler.h"
#include "token.h"
#include "memory.h"

void consume_new_token(cunit* cu, token* t);
static inline void void_lookahead_token(cunit* cu){
    cu->lookahead_head--;
    if(cu->lookahead_head != cu->lookahead_store){
         memcpy(cu->lookahead_store, cu->lookahead_store + 1,
           (cu->lookahead_head - cu->lookahead_store) * sizeof(token));
    }
}
static inline void consume_lookahead_token(cunit* cu, token* t){
    *t =  cu->lookahead_store[0];
    cu->lookahead_head--;
    if(cu->lookahead_head != cu->lookahead_store){
         memcpy(cu->lookahead_store, cu->lookahead_store + 1,
           (cu->lookahead_head - cu->lookahead_store) * sizeof(token));
    }
}
static inline void lookahead_token(cunit* cu, token* t, int a){
    a--;
    while(cu->lookahead_head <= &cu->lookahead_store[a]){
        consume_new_token(cu, cu->lookahead_head);
        cu->lookahead_head++;
    }
    *t = cu->lookahead_store[a];
}
static inline void consume_token(cunit* cu, token* t){
    if(cu->lookahead_head == cu->lookahead_store){
        consume_new_token(cu, t);
    }
    else{
       consume_lookahead_token(cu, t);
    }
}
static inline void clear_lookahead(cunit* cu){
    cu->lookahead_head = cu->lookahead_store;
}
static inline int get_lookup_count(cunit* cu){
    return (int)(cu->lookahead_head - cu->lookahead_store);
}

char* store_string(cunit* cu, char* str, char* str_end);
char* store_zero_terminated_string(cunit* cu, char* str);


