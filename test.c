#include<stdio.h>
#include<string.h>

int main(){

    unsigned char a;
    unsigned char b;
    unsigned char c;
    char data[]="123do";
    char buf[128];
    a=0x34;
    b=0x30;
    printf("a=%x\n",a);

    c = a & 0xf0;
    printf("b=%x c=%x\n",b,c);

    sprintf(buf,"4%s",data);

    printf("%x\n",buf[0]);

    if(c == 0x30){
	printf("gg\n");
    }
    

    return 0;
}
