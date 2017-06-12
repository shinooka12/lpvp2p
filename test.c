#include<stdio.h>
#include<string.h>

int main(){

    unsigned char a;
    unsigned char b;
    unsigned char c;
    char data[]="123do";
    char buf[128];
    char tmp[2][256] = {
	"gogogo","backback"
    };

    a=0x34;
    b=0x30;
    printf("a=%x\n",a);

    c = a & 0xf0;
    printf("b=%x c=%x\n",b,c);

    sprintf(buf,"%c%s",a,data);

    printf("%s\n",buf);
    printf("buf[0]=%x buf[1]=%x %x %x %x %x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);

    if(buf[0] == a){
	printf("gg\n");
    }

    char tmp1[128] ="abc";

    printf("%d\n",sizeof(tmp1));



    return 0;
}
