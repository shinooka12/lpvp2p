#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<pthread.h>

#define SERVICE_PORT 12345
#define PARENT_MAX 1
#define CHILD_MAX 2
#define BUFSIZE 256


typedef struct{
    int s;
    struct sockaddr_in senderinfo;
}sock_t;

typedef struct{
    char parent[PARENT_MAX][BUFSIZE];
    char child[CHILD_MAX][BUFSIZE];
}node_t;
node_t node;


void reset_node();
int start_p2p();
void connect_recv(sock_t *);


int main(){

    reset_node();
    start_p2p();

    return 0;

}

void reset_node(){

    int i;

    for(i=0;i<PARENT_MAX;i++){
	sprintf(node.parent[i],"nothing");
    }
    for(i=0;i<CHILD_MAX;i++){
	sprintf(node.child[i],"nothing");
    }

}

int start_p2p(){

    pthread_t worker;
    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    socklen_t addrlen;
    sock_t new_s;
    char recvbuf[BUFSIZE];
    char senderstr[BUFSIZE];
    int n;

    sock = socket(AF_INET,SOCK_DGRAM,0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVICE_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock,(struct sockaddr *)&addr,sizeof(addr));

    while(1){

	memset(recvbuf,0,sizeof(recvbuf));

	//recvfromでUDPソケットからデータを受信
	addrlen = sizeof(senderinfo);
	n = recvfrom(sock,recvbuf,sizeof(recvbuf)-1,0,(struct sockaddr *)&senderinfo,&addrlen);
	//送信元の情報を出力
	inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderstr));

	new_s.s = sock;
	new_s.senderinfo = senderinfo;

	printf("[MAIN THREAD] recieve packet\n");
	printf("recvfrom: %s  port: %d  recv command: %s\n",senderstr,ntohs(senderinfo.sin_port),recvbuf);
	pthread_create(&worker,NULL,(void *)connect_recv,(void *)&new_s);
	printf("[MAIN THREAD] CREATE THREAD [%u]\n",worker);

	pthread_join(worker,NULL);
	printf("[MAIN THREAD] JOIN [%u]\n",worker);
	printf("PARENT: %s  CHILD: %s %s\n",node.parent[0],node.child[0],node.child[1]);

    }

    close(sock);

    return 0;
}


void connect_recv(sock_t *new_s){

    int sock;
    int i;
    int flag;
    struct sockaddr_in senderinfo;
    char sendbuf[BUFSIZE];
    char senderstr[BUFSIZE];

    sock = new_s->s;
    senderinfo = new_s->senderinfo;

    inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderstr));
    //回線速度が自分の方が早いとき
    //まだ分岐しない
    flag = -1;
    for(i=0;i<CHILD_MAX;i++){
	if(strcmp(node.child[i],"nothing") == 0 && flag != 0){
	    sprintf(node.child[i],senderstr);
	    flag = 0;
	}
    }

    if(flag == 0){

	//UDPで返信
	sprintf(sendbuf,"CONNECT ACK");
	sendto(sock,sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));

	//送信先の情報を出力
	printf("sendto: %s  port: %d  send command: %s\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);
    }else{	//子ノードがいっぱいで接続できない
	
	//UDPで返信
	sprintf(sendbuf,"CONNECT REFUSE");
	sendto(sock,sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));

	//送信先の情報を出力
	printf("sendto: %s  port: %d  send command: %s\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);
    }

}
