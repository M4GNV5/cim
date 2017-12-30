#pragma once
#include "compiler.h"

typedef enum tokens_type_e{
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_LITERAL,
    TOKEN_BINARY_LITERAL,
    TOKEN_EOF,
    TOKEN_PLUS,
    TOKEN_DOUBLE_PLUS,
    TOKEN_PLUS_EQUALS,
    TOKEN_MINUS,
    TOKEN_DOUBLE_MINUS,
    TOKEN_MINUS_EQUALS,
    TOKEN_EXCLAMATION_MARK,
    TOKEN_EXCLAMATION_MARK_EQUALS,
    TOKEN_STAR,
    TOKEN_STAR_EQUALS,
    TOKEN_EQUALS,
    TOKEN_DOUBLE_EQUALS,
    TOKEN_PAREN_OPEN,
    TOKEN_PAREN_CLOSE,
    TOKEN_BRACKET_OPEN,
    TOKEN_BRACKET_CLOSE,
    TOKEN_BRACE_OPEN,
    TOKEN_BRACE_CLOSE,
    TOKEN_PERCENT,
    TOKEN_PERCENT_EQUALS,
    TOKEN_SLASH,
    TOKEN_SLASH_EQUALS,
	TOKEN_AND,
	TOKEN_DOUBLE_AND,
	TOKEN_AND_EQUALS,
	TOKEN_PIPE,
	TOKEN_DOUBLE_PIPE,
	TOKEN_PIPE_EQUALS,
	TOKEN_TILDE,
	TOKEN_TILDE_EQUALS,
	TOKEN_DOLLAR,
	TOKEN_COMMA,
	TOKEN_SEMICOLON,
	TOKEN_HASH,
	TOKEN_DOUBLE_HASH,
    TOKEN_LESS_THAN,
    TOKEN_DOUBLE_LESS_THAN,
    TOKEN_GREATER_THAN,
    TOKEN_DOUBLE_GREATER_THAN,
    TOKEN_DOT,
    TOKEN_ARROW,
}token_type;

typedef struct token_t{
	token_type type;
    ureg str;
}token;

void get_token(cunit* cu, token* t);
void print_token(cunit* cu, token* t);
void print_rel_str(cunit* cu, ureg str);
ureg store_string(cunit* cu, char* str, char* str_end);
void print_token(cunit* cu, token* t);
void get_token(cunit* cu, token* tok);