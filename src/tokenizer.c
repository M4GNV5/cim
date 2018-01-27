#include "parser.h"
#include "tokenizer.h"
#include "ast.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "error.h"
#include "compiler.h"
#include "token_strings.h"
#include <stdarg.h>
#define ASSERT_NEOF(c) do{if((c) == '\0'){CIM_ERROR("Unexpected EOF");}}while(0)
static void populate_file_buffer(cunit* cu, u8* pop_start){
    ureg s = fread(pop_start,
                   1, cu->tknzr.file_buffer.end - pop_start,
                   cu->tknzr.file);
    cu->tknzr.file_buffer.head =  pop_start + s;
}
void tokenizer_open_file(cunit* cu, char* filename) {
    if(cu->tknzr.file != NULL){
        fclose(cu->tknzr.file);
    }
    cu->tknzr.file = fopen(filename, "r");
    if(cu->tknzr.file == NULL){
        printf("Failed to open file %s", filename);
        exit(-1);
    }
    cu->tknzr.filename = filename;
    cu->tknzr.token_start = cu->tknzr.token_buffer;
    cu->tknzr.token_end = cu->tknzr.token_start;
    cu->tknzr.token_start->column = 0;
    cu->tknzr.token_start->line = 0;
    cu->tknzr.curr = (char*)cu->tknzr.file_buffer.start;
    populate_file_buffer(cu, cu->tknzr.file_buffer.start);
}
void tokenizer_close_file(cunit* cu){
    fclose(cu->tknzr.file);
    cu->tknzr.filename = NULL;
    cu->tknzr.file = NULL;
}
static inline void unread_char(cunit* cu){
     cu->tknzr.curr--;
}
static inline char peek_char(cunit* cu){
    if((u8*)cu->tknzr.curr != cu->tknzr.file_buffer.head){
        return *cu->tknzr.curr;
    }
    else{
        cu->tknzr.curr = (char*)cu->tknzr.file_buffer.start;
        populate_file_buffer(cu, cu->tknzr.file_buffer.start);
        if((u8*)cu->tknzr.curr != cu->tknzr.file_buffer.head){
            return *cu->tknzr.curr;
        }
        else{
            return '\0';
        }
    }
}
static inline void void_peek(cunit* cu){
    cu->tknzr.curr+=1;
}
static inline char peek_string_char(cunit* cu, char** str_start){
    if((u8*)cu->tknzr.curr != cu->tknzr.file_buffer.head) {
        return *cu->tknzr.curr;
    }
    else{
        ureg size = cu->tknzr.curr - *str_start;
        if((u8*)*str_start != cu->tknzr.file_buffer.start) {
            cu->tknzr.curr = (char*)cu->tknzr.file_buffer.start + size;
            memmove(cu->tknzr.file_buffer.start, *str_start, size);
            populate_file_buffer(cu, (u8*)cu->tknzr.curr);
            if ((u8 *) cu->tknzr.curr != cu->tknzr.file_buffer.head) {
                *str_start = (char *) cu->tknzr.file_buffer.start;
                return *cu->tknzr.curr;
            }
        }
        //not enough space
        ureg curr_offs = (u8*)cu->tknzr.curr - cu->tknzr.file_buffer.start;
        dbuffer_grow(&cu->tknzr.file_buffer);
        populate_file_buffer(cu, cu->tknzr.file_buffer.start + size);
        cu->tknzr.curr = (char*)(cu->tknzr.file_buffer.start + curr_offs);
        *str_start = (char*)cu->tknzr.file_buffer.start;
        if((u8*)cu->tknzr.curr != cu->tknzr.file_buffer.head) {
            return *cu->tknzr.curr;
        }
        else{
            return '\0';
        }
    };
}
void display_string_store(cunit* cu){
    char** t = (void*)cu->string_ptrs.start;
    while(t != (void*)cu->string_ptrs.head){
        printf("%s\n",*t);
        t++;
    }
}
static inline int cmp_unended_string_with_stored(char* str_start, const char* str_end, char* stored){
	for(;;){
        if(str_start == str_end){
            if(*stored == '\0')return 0;
            return -*stored;
        }
		if(*str_start != *stored){
			return  *str_start - *stored;
		}
		str_start++;
		stored++;
	}
}
void add_keyword(cunit* cu, const char* str){
    char** sptrs_start = (char**)cu->string_ptrs.start;
	char** sptrs_end = (char**)cu->string_ptrs.head;
	char** pivot;
    int res;
    for(;;) {
        if(sptrs_end == sptrs_start) {
            if (sptrs_start == (char**)cu->string_ptrs.head){
                *(const char**)dbuffer_claim_small_space(&cu->string_ptrs,
                                                   sizeof(char**)) = str;
            }
            else{
                if(strcmp(str, *sptrs_end) == 0) CIM_ERROR("keyword already present");
                 dbuffer_insert_at(&cu->string_ptrs, &str, sptrs_end, sizeof(char*));
            }
            return;
        }
        pivot = sptrs_start + (sptrs_end - sptrs_start) / 2;
        res = strcmp(str, *pivot);
        if(res < 0){
			sptrs_end = pivot;
		}
		else if(res > 0){
			sptrs_start = pivot +1;
		}
		else{
			CIM_ERROR("keyword already present");
		}
    }
}
char* store_string(cunit* cu, char* str, char* str_end){
	//we do this ahead so we don't have to worry about invalidating pointers
	dbuffer_make_small_space(&cu->string_ptrs, sizeof(char*));
	char** sptrs_start = (char**)cu->string_ptrs.start;
	char** sptrs_end = (char**)cu->string_ptrs.head;
	char** pivot;
	int res;
	for(;;){
		if(sptrs_end == sptrs_start){
            if(sptrs_start != (char**)cu->string_ptrs.head &&
               cmp_unended_string_with_stored(str, str_end, *sptrs_end) == 0)
            {
                return *sptrs_end;
            }
            ureg str_size = str_end - str;
            char* tgt = sbuffer_append(&cu->data_store, str_size + 1); //+1 for \0
            memcpy(tgt, str, str_size);
            *(tgt + str_size) = '\0';
            dbuffer_insert_at(&cu->string_ptrs, &tgt, sptrs_end, sizeof(char*));
            return tgt;
        }
        pivot = sptrs_start + (sptrs_end - sptrs_start) / 2;
        res = cmp_unended_string_with_stored(str, str_end, *pivot);
		if(res < 0){
			sptrs_end = pivot;
		}
		else if(res > 0){
			sptrs_start = pivot +1;
		}
		else{
			return *pivot;
		}
	}
}

void consume_new_token(cunit* cu, token* tok, token* next){
	char curr = peek_char(cu);
    void_peek(cu);
    next->line = tok->line;
	switch(curr){
		case '$': tok->type = TOKEN_DOLLAR;break;
        case '(': tok->type = TOKEN_PAREN_OPEN;break;
        case ')': tok->type = TOKEN_PAREN_CLOSE;break;
        case '{': tok->type = TOKEN_BRACE_OPEN;break;
        case '}': tok->type = TOKEN_BRACE_CLOSE;break;
        case '[': tok->type = TOKEN_BRACKET_OPEN;break;
        case ']': tok->type = TOKEN_BRACKET_CLOSE;break;
        case ',': tok->type = TOKEN_COMMA;break;
		case ';': tok->type = TOKEN_SEMICOLON; break;
        case '.': tok->type = TOKEN_DOT; break;
        case ':': tok->type = TOKEN_COLON; break;
        case '\0':tok->type = TOKEN_EOF;break;
        case '\t': {
            tok->column++;
            curr = peek_char(cu);
            while(curr == '\t'){
                tok->column++;
                void_peek(cu);
                curr = peek_char(cu);
            }
            return consume_new_token(cu, tok, next);
        }
        case ' ': {
            tok->column++;
            curr = peek_char(cu);
            while(curr == ' '){
                tok->column++;
                void_peek(cu);
                curr = peek_char(cu);
            }
            return consume_new_token(cu, tok, next);
        }
        case '\n':{
            tok->line++;
            tok->column=0;
            return consume_new_token(cu, tok, next);
        }
        case '*': {
            char peek = peek_char(cu);
            if(peek == '=') {
                void_peek(cu);
                tok->type = TOKEN_STAR_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_STAR;
                break;
            }
        }
        case '+': {
            char peek = peek_char(cu);
            if(peek == '+') {
                void_peek(cu);
                tok->type = TOKEN_DOUBLE_PLUS;
                next->column = tok->column+2;
                return;
            }
            else if(peek == '='){
                void_peek(cu);
                tok->type = TOKEN_PLUS_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_PLUS;
                break;
            }
        }
	    case '-': {
            char peek = peek_char(cu);
            if(peek == '-') {
                void_peek(cu);
                tok->type = TOKEN_DOUBLE_MINUS;
                next->column = tok->column+2;
                return;
            }
            else if(peek == '='){
                void_peek(cu);
                tok->type = TOKEN_MINUS_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else if(peek == '>'){
                void_peek(cu);
                tok->type = TOKEN_ARROW;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_MINUS;
                break;
            }
        }
        case '!': {
            char peek = peek_char(cu);
            if(peek == '=') {
                void_peek(cu);
                tok->type = TOKEN_EXCLAMATION_MARK_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_EXCLAMATION_MARK;
                break;
            }
        }
        case '|': {
            char peek = peek_char(cu);
            if(peek == '|') {
                void_peek(cu);
                tok->type = TOKEN_DOUBLE_PIPE;
                next->column = tok->column+2;
                return;
            }
            else if(peek == '='){
                void_peek(cu);
                tok->type = TOKEN_PIPE_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_PIPE;
                break;
            }
        }
		case '&': {
            char peek = peek_char(cu);
            if(peek == '&') {
                void_peek(cu);
                tok->type = TOKEN_DOUBLE_AND;
                next->column = tok->column+2;
                return;
            }
            else if(peek == '='){
                void_peek(cu);
                tok->type = TOKEN_AND_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_AND;
                break;
            }
        }
        case '^': {
            char peek = peek_char(cu);
            if(peek == '^') {
                void_peek(cu);
                tok->type = TOKEN_DOUBLE_CARET;
                next->column = tok->column+2;
                return;
            }
            else if(peek == '='){
                void_peek(cu);
                tok->type = TOKEN_CARET_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_CARET;
                break;
            }
        } return;
        case '~': {
            char peek = peek_char(cu);
            if(peek == '=') {
                void_peek(cu);
                tok->type = TOKEN_TILDE_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_TILDE;
                break;
            }
        }
       case '=': {
            char peek = peek_char(cu);
            if(peek == '=') {
                void_peek(cu);
                tok->type = TOKEN_DOUBLE_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                 tok->type = TOKEN_EQUALS;
                break;
            }
        }
        case '/': {
            char peek = peek_char(cu);
            if(peek == '/'){
                do{
                    void_peek(cu);
                    curr = peek_char(cu);
                    while(curr == '\\'){
                        void_peek(cu);
                        curr = peek_char(cu);
                        ASSERT_NEOF(curr);
                        void_peek(cu);
                        curr = peek_char(cu);
                        ASSERT_NEOF(curr);
                    }
                }while(curr != '\n');
                void_peek(cu);
                tok->line++;
                tok->column = 0;
                return consume_new_token(cu, tok, next);
            }
            if(peek == '*'){
                tok->column+=2;
                do{
                    do{
                        void_peek(cu);
                        tok->column++;
                        curr = peek_char(cu);
                        while(curr == '\\'){
                            void_peek(cu);
                            curr = peek_char(cu);
                            ASSERT_NEOF(curr);
                            void_peek(cu);
                            tok->column+=2;
                            curr = peek_char(cu);
                        }
                        while(curr == '\n'){
                            void_peek(cu);
                            curr = peek_char(cu);
                            tok->line++;
                            tok->column = 0;
                        }
                        ASSERT_NEOF(curr);
                    }while(curr != '*');
                    void_peek(cu);
                    tok->column++;
                    peek = peek_char(cu);
                }while(peek != '/');
                void_peek(cu);
                tok->column++;
                return consume_new_token(cu, tok, next);
            }
            if(peek == '='){
                void_peek(cu);
                tok->type = TOKEN_SLASH_EQUALS;
                next->column= tok->column + 2;
                return;
            }
            else{
                tok->type = TOKEN_SLASH;
                break;
            }
        }
		case '%': {
            char peek = peek_char(cu);
            if(peek == '='){
                void_peek(cu);
                tok->type = TOKEN_PERCENT_EQUALS;
                next->column = tok->column + 2;
                return;
            }
            else{
                tok->type = TOKEN_PERCENT;
                break;
            }
        }
        case '#': {
            char peek = peek_char(cu);
            if(peek == '#'){
                void_peek(cu);
                tok->type = TOKEN_DOUBLE_HASH;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_HASH;
                break;
            }
        }
        case '<': {
            char peek = peek_char(cu);
            if(peek == '<'){
                void_peek(cu);
                peek = peek_char(cu);
                if(peek == '='){
                    void_peek(cu);
                    tok->type = TOKEN_DOUBLE_LESS_THAN_EQUALS;
                    next->column = tok->column+3;
                    return;
                }
                else{
                    tok->type = TOKEN_DOUBLE_LESS_THAN;
                    next->column = tok->column+2;
                    return;
                }
            }
            else if(peek == '='){
                tok->type = TOKEN_LESS_THAN_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_LESS_THAN;
                break;
            }
        } return;
        case '>': {
            char peek = peek_char(cu);
            if(peek == '>'){
                void_peek(cu);
                peek = peek_char(cu);
                if(peek == '='){
                    void_peek(cu);
                    tok->type = TOKEN_DOUBLE_GREATER_THAN_EQUALS;
                    next->column = tok->column+3;
                    return;
                }
                else{
                    tok->type = TOKEN_DOUBLE_GREATER_THAN;
                    next->column = tok->column+2;
                    return;
                }
            }
            else if(peek == '='){
                tok->type = TOKEN_GREATER_THAN_EQUALS;
                next->column = tok->column+2;
                return;
            }
            else{
                tok->type = TOKEN_GREATER_THAN;
                break;
            }
        } return;
        case '\'':{
            char* str_start = cu->tknzr.curr-1;
            do{
                void_peek(cu);
                next->column++;
                curr = peek_string_char(cu, &str_start);
                ASSERT_NEOF(curr);
                if(curr == '\\'){
                    void_peek(cu);
                    curr = peek_string_char(cu, &str_start);
                    ASSERT_NEOF(curr);
                    void_peek(cu);
                    curr = peek_string_char(cu, &str_start);
                    ASSERT_NEOF(curr);
                    next->column+=2;
                }
                if(curr == '\n'){
                    curr = peek_string_char(cu, &str_start);
                    ASSERT_NEOF(curr);
                }
            }while(curr != '\'');
            tok->str = store_string(cu, str_start, cu->tknzr.curr);
            next->column = tok->column + 2 + cu->tknzr.curr - str_start;
            tok->type = TOKEN_BINARY_LITERAL;
        } return;
        case '\"':{
            char* str_start = cu->tknzr.curr-1;
            do{
                void_peek(cu);
                curr = peek_string_char(cu, &str_start);
                ASSERT_NEOF(curr);
                if(curr == '\\'){
                    void_peek(cu);
                    curr = peek_string_char(cu, &str_start);
                    ASSERT_NEOF(curr);
                    void_peek(cu);
                    curr = peek_string_char(cu, &str_start);
                    ASSERT_NEOF(curr);
                }
                if(curr == '\n'){
                    next->line++;
                    void_peek(cu);
                    curr = peek_string_char(cu, &str_start);
                    ASSERT_NEOF(curr);
                }
            }while(curr != '\"');
            tok->str = store_string(cu, str_start, cu->tknzr.curr);
            next->column = tok->column + 2 + cu->tknzr.curr - str_start;
            tok->type = TOKEN_LITERAL;
        } return;
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's':
        case 't':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
        case 'z':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I':
        case 'J':
        case 'K':
        case 'L':
        case 'M':
        case 'N':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
        case 'T':
        case 'U':
        case 'V':
        case 'W':
        case 'X':
        case 'Y':
        case 'Z':
        case '_':
        {
            char* str_start = cu->tknzr.curr-1;
            curr = peek_string_char(cu, &str_start);
            while((curr >= 'a' && curr <= 'z') ||
                   (curr >= 'A' && curr <= 'Z') ||
                   (curr >= '0' && curr <= '9') ||
                   (curr == '_'))
            {
                void_peek(cu);
                curr = peek_string_char(cu, &str_start);
            }
            tok->str = store_string(cu, str_start, cu->tknzr.curr);
            next->column = tok->column + cu->tknzr.curr - str_start;
            tok->type = TOKEN_STRING;
            return;
        }
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        {
            char* str_start = cu->tknzr.curr-1;
            curr = peek_string_char(cu, &str_start);
            while(curr >= '0' && curr <= '9'){
                void_peek(cu);
                curr = peek_string_char(cu, &str_start);
                ASSERT_NEOF(curr);
            }
            tok->str = store_string(cu, str_start, cu->tknzr.curr);
            next->column = tok->column + cu->tknzr.curr - str_start;
            tok->type = TOKEN_NUMBER;
        } return;
		default:{
            printf("unknown token: %c", curr);
        }assert(false);
	}
    next->column = tok->column+1;
}
const char* get_token_type_str(token_type t){
    static char buff[6] = {'\''};
    if(token_strings[t]!=0){
        ureg len = strlen(token_strings[t]);
        memcpy(buff + 1, token_strings[t], len);
        buff[len+1]='\'';
        return buff;
    }
    else{
        switch  (t){
            case TOKEN_EOF: return "end of file";
            case TOKEN_NUMBER: return "number";
            case TOKEN_STRING: return "string";
            case TOKEN_BINARY_LITERAL: return "binary literal";
            case TOKEN_LITERAL: return "literal";
            default: CIM_ERROR("Failed to print the desired token");
        }
    }
};
const char* make_token_string(cunit* cu, token* t){
    if(token_strings[t->type]!=0)return get_token_type_str(t->type);
    ureg len = strlen(t->str);
    char* buff = sbuffer_append(&cu->data_store, len +3);
    cu->data_store.last->head = (u8*)buff;
    memcpy(buff + 1, t->str, len);
    buff[len+2] = '\0';
    switch (t->type){
        case TOKEN_EOF: return "end of file";
        case TOKEN_LITERAL:
        case TOKEN_NUMBER:
        case TOKEN_STRING: buff[0]='"';buff[len+1]='"';return buff;
        case TOKEN_BINARY_LITERAL: buff[0]='\'';buff[len+1]='\'';return buff;
        default: CIM_ERROR("Failed to print the desired token");
    }
};
void error(cunit* cu, token* t, char* str, ...){
    ureg pos;
    ureg line_size;
    ureg st, en;
    printf("%s:%llu:%llu: ",
               cu->tknzr.filename,
               t->line + 1,
               t->column);
    va_list vl;
    va_start(vl, str);
    vprintf(str, vl);
    va_end(vl);
    putchar('\n');
    for(ureg i = 0; i<line_size;i++){
        putchar(cu->tknzr.curr[i]);
    }
    putchar('\n');
    for(ureg i = 0; i!= st; i++)putchar(' ');
    for(ureg i = st; i!= en; i++)putchar('^');
    fflush(stdout);
    CIM_EXIT;
}