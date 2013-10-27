
#include "string.h"


int strcmp(const char *a, const char *b)
{
	asm(
        "strcmp_lop:                \n"
        "   ldrb    r2, [r0],#1     \n"
        "   ldrb    r3, [r1],#1     \n"
        "   cmp     r2, #1          \n"
        "   it      hi              \n"
        "   cmphi   r2, r3          \n"
        "   beq     strcmp_lop      \n"
		"	sub     r0, r2, r3  	\n"
        "   bx      lr              \n"
		:::
	);
}


size_t strlen(const char *s)
{
	asm(
		"	sub  r3, r0, #1			\n"
        "strlen_loop:               \n"
		"	ldrb r2, [r3, #1]!		\n"
		"	cmp  r2, #0				\n"
        "   bne  strlen_loop        \n"
		"	sub  r0, r3, r0			\n"
		"	bx   lr					\n"
		:::
	);
}

char* itoa(int value, char* str)//only support base=10
{
    int base = 10;
    int divideNum = base;
    int i=0;
    while(value/divideNum > 0)
    {
        divideNum*=base;
    }
    if(value < 0)
    {
        str[0] = '-';
        i++;
    }
    while(divideNum/base > 0)
    {
        divideNum/=base;
        str[i++]=value/divideNum+48;
        value%=divideNum;
    }
    str[i]='\0';
    return str;

}
