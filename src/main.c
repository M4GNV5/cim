#include "parser.h"
#include "tokenizer.h"
#include <string.h>
#include <stdio.h>
int main(){
	cunit cu;
	init(&cu);
    const int cnt = 1;
    char* pre_str = "int a = ";
    ureg pre_str_len = strlen(pre_str);
	char* str = " -1 + 17 / 3";
    ureg str_size = strlen(str);
    char* str_comb = malloc(pre_str_len + str_size * cnt + 3);
    memcpy(str_comb, pre_str, pre_str_len);
    char* str_comb_it = str_comb + pre_str_len;
    char* str_comb_it_end = str_comb_it + str_size * cnt;
    while(str_comb_it != str_comb_it_end){
        memcpy(str_comb_it, str, str_size);
        str_comb_it+= str_size;
    }
    str_comb_it_end[0] = ' ';
    str_comb_it_end[1] = ';';
    str_comb_it_end[2] = '\0';
    printf("input:\n%s\n\n", str_comb);
	cu.str = str_comb;
	cu.pos = str_comb;
	token t;
    parse(&cu, str_comb);
    printf("output:\n");
    print_ast(&cu);
	return 0;
}
