#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<pthread.h>

#define KNOWN_MAX 2
#define SERVICE_PORT 12345
#define PARENT_MAX 1
#define CHILD_MAX 2
#define BUFSIZE 256
#define THREAD_NUM 2
#define TARGET "192.168.1.2"

#define CON "CONNECT"
#define CONACK "CONNECT ACK"
#define CONREF "CONNECT REFUSE"


typedef struct{
    int s;
    struct sockaddr_in senderinfo;
}sock_t;

typedef struct{
    char parent[PARENT_MAX][BUFSIZE];
    char child[CHILD_MAX][BUFSIZE];
    int parent_flag;
}node_t;

typedef struct{
    char node_ip[BUFSIZE];
}list_t;

node_t node;
list_t node_list[KNOWN_MAX];
int first_connect;

void reset_node();
int start_p2p();
void connect_recv(sock_t *);
void connect_parent(sock_t *new_s);
void timer_handler(int );


int main(){

    reset_node();
    start_p2p();

    return 0;

}

void reset_node(){

    int i;

    first_connect = -1;
    for(i=0;i<PARENT_MAX;i++){
	sprintf(node.parent[i],"nothing");
    }
    for(i=0;i<CHILD_MAX;i++){
	sprintf(node.child[i],"nothing");
    }
    node.parent_flag = -1;

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
    int flag;

    printf("[MAIN THREAD] connect parent\n");
    pthread_create(&worker,NULL,(void *)connect_parent,(void *)&new_s);
    printf("[MAIN THREAD] CREATE THREAD [%u]\n",worker);


    sock = socket(AF_INET,SOCK_DGRAM,0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVICE_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock,(struct sockaddr *)&addr,sizeof(addr));
    flag = -1;


    while(1){

	memset(recvbuf,0,sizeof(recvbuf));


	//recvfromでUDPソケットからデータを受信
	addrlen = sizeof(senderinfo);
	n = recvfrom(sock,recvbuf,sizeof(recvbuf)-1,0,(struct sockaddr *)&senderinfo,&addrlen);


	//送信元の情報を出力
	inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderstr));

	new_s.s = sock;
	new_s.senderinfo = senderinfo;


	if(strcmp(recvbuf,CON) == 0){
	    printf("[MAIN THREAD] recieve packet\n");
	    printf("recvfrom: %s  port: %d  recv command: %s\n",senderstr,ntohs(senderinfo.sin_port),recvbuf);
	    pthread_create(&worker,NULL,(void *)connect_recv,(void *)&new_s);
	    printf("[MAIN THREAD] CREATE THREAD [%u]\n",worker);
	}


	pthread_detach(worker);

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
    for(i=0;i<CHILD_MAX;i++){
	if(strcmp(node.child[i],senderstr) == 0){
	    sprintf(sendbuf,CONREF);
	    sendto(sock,sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));
	    return;

	}
    }

    flag = -1;
    for(i=0;i<CHILD_MAX;i++){
	if(strcmp(node.child[i],"nothing") == 0 && flag != 0){
		sprintf(node.child[i],senderstr);
		flag = 0;
	}

	if(strcmp(node.child[i],senderstr) == 0);
    }

    if(flag == 0){

	//UDPで返信
	sprintf(sendbuf,CONACK);
	sendto(sock,sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));

	//送信先の情報を出力
	printf("sendto: %s  port: %d  send command: %s\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);
    }else{	//子ノードがいっぱいで接続できない
	
	//UDPで返信
	sprintf(sendbuf,CONREF);
	sendto(sock,sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));

	//送信先の情報を出力
	printf("sendto: %s  port: %d  send command: %s\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);
    }

	printf("PARENT: %s  CHILD: %s %s\n",node.parent[0],node.child[0],node.child[1]);

}


void connect_parent(sock_t *new_s){


    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    socklen_t senderinfolen;
    char target_ip[BUFSIZE];
    char sendbuf[BUFSIZE];
    char recvbuf[BUFSIZE];
    int i;
    int n;
    int flag;
    //タイマ割り込みを発生されるための変数
    fd_set fds,readfds;
    int maxfd;
    int m;
    struct timeval tv;

    sock = socket(AF_INET,SOCK_DGRAM,0);

    tv.tv_sec = 3;
    tv.tv_usec = 0;

    while(1){

	if(first_connect == -1){
	    strcpy(target_ip,TARGET);
	    first_connect = 0;
	}else{
	    memset(target_ip,0,sizeof(target_ip));
	    strcpy(target_ip,TARGET);
	    tv.tv_sec = 10;
	    tv.tv_usec = 0;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVICE_PORT);
	inet_pton(AF_INET,target_ip,&addr.sin_addr.s_addr);
	bind(sock,(struct sockaddr *)&addr,sizeof(addr));

	//親がいなければflag=-1となり接続できる 親がいる場合はflag=0なのでreturnされる
	flag = 0;
	for(i=0;i<PARENT_MAX;i++){
	    if(strcmp(node.parent[i],"nothing") == 0){
		flag = -1;
	    }
	}
	if(flag == 0){

	    printf("CONNECT MAX PARENT. CANCEL CONNECT\n");
	    node.parent_flag = 0;
	    return;
	}


	//sendtoでCONNECTを送信
	sprintf(sendbuf,CON);
	n = sendto(sock,sendbuf,sizeof(sendbuf)-1,0,(struct sockaddr *)&addr,sizeof(addr));
	printf("\nsend CONNECT [IP:%s]\n",target_ip);
	if(n < 1){
	    perror("sendto");
	    return;
	}


	//タイマ割り込みを発生されるための処理
	FD_ZERO(&readfds);
	FD_SET(sock,&readfds);
	maxfd = sock;

	memcpy(&fds,&readfds,sizeof(fd_set));
	m = select(maxfd+1,&fds,NULL,NULL,&tv);

	if(m == 0){
	    printf("CONNECT PARENT session time out\n");
	}


	//返信待ち
	memset(recvbuf,0,sizeof(recvbuf));
	senderinfolen = sizeof(senderinfo);
	printf("wait reply from %s\n",target_ip);
	if(FD_ISSET(sock,&fds)){
	    recvfrom(sock,recvbuf,sizeof(recvbuf),0,(struct sockaddr *)&senderinfo,&senderinfolen);

	    //CONNECT ACKの時は親のリストに追加
	    if(strcmp(recvbuf,CONACK) == 0){
		flag = -1;

		printf("\nCONACK receive [IP:%s]\n",target_ip);
		for(i=0;i<PARENT_MAX;i++){
		    if(strcmp(node.parent[i],"nothing") == 0 && flag != 0){
			sprintf(node.parent[i],target_ip);
			flag = 0;
		    }
		}
	    }else if(strcmp(recvbuf,CONREF) == 0){
		printf("CONNECT REFUSE[IP:%s]  plz connect other node\n",target_ip);
	    }else{
		printf("RECIEVE UNKNOWN COMMAND[IP:%s]",target_ip);
	    }
	}

	printf("end recieve\n");
	printf("PARENT: %s  CHILD: %s %s\n",node.parent[0],node.child[0],node.child[1]);

    }

}

