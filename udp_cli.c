#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#define SERVICE_PORT 12345

void print_my_port_num(int sock){

    struct sockaddr_in s;
    socklen_t sz = sizeof(s);
    getsockname(sock,(struct sockaddr *)&s,&sz);
    printf("my port: %d\n",ntohs(s.sin_port));

}

int main(int argc,char *argv[]){

    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    socklen_t senderinfolen;
    char sendbuf[256];
    char buf[256];
    int n;

    if(argc != 2){
	fprintf(stderr,"Usage : %s dst ipaddr\n",argv[0]);
	return 1;
    }

    sock = socket(AF_INET,SOCK_DGRAM,0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVICE_PORT);
    inet_pton(AF_INET,argv[1],&addr.sin_addr.s_addr);


    //sendtoでデータを送信
    sprintf(sendbuf,"CONNECT");
    n = sendto(sock,sendbuf,sizeof(sendbuf)-1,0,(struct sockaddr *)&addr,sizeof(addr));
    if(n < 1){
	perror("sendto");
	return 1;
    }



    print_my_port_num(sock);


    memset(buf,0,sizeof(buf));
    senderinfolen = sizeof(senderinfo);
    recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr *)&senderinfo,&senderinfolen);
    printf("recv : %s\n",buf);

    close(sock);

    return 0;

}
