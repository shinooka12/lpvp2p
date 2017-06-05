#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#define SERVICE_PORT 12345

int main(){

    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    socklen_t addrlen;
    char buf[256];
    char sendbuf[256];
    char senderstr[16];
    int n;

    sock = socket(AF_INET,SOCK_DGRAM,0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVICE_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock,(struct sockaddr *)&addr,sizeof(addr));

    while(1){

	memset(buf,0,sizeof(buf));

	//recvfromでUDPソケットからデータを受信
	addrlen = sizeof(senderinfo);
	n = recvfrom(sock,buf,sizeof(buf)-1,0,(struct sockaddr *)&senderinfo,&addrlen);
	printf("recv : %s\n",buf);

	//送信元の情報を出力
	inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderstr));
	printf("recvfrom: %s  port: %d\n",senderstr,ntohs(senderinfo.sin_port));

	//UDPで返信
	sprintf(sendbuf,"CONNECT ACK");
	sendto(sock,sendbuf,n,0,(struct sockaddr *)&senderinfo,addrlen);
	printf("send : %s\n",sendbuf);

	//送信元の情報を出力
	printf("sendto: %s  port: %d\n",senderstr,ntohs(senderinfo.sin_port));

    }

    close(sock);

    return 0;

}
