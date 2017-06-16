#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<malloc.h>

#define KNOWN_MAX 5
#define SERVICE_PORT 12345
#define PARENT_MAX 1
#define CHILD_MAX 2
#define MAX_KEY 10
#define BUFSIZE 256
#define THREAD_NUM 2
#define TARGET "192.168.1.2"

#define CON 0x10	//CONNECT
#define CONACK 0x20	//CONNECT ACK
#define CONREF 0x30	//CONNECT REFUSE
#define ACK 0x40	//ACK
#define OTHERINFO 0x50	//OTHER NODE INFO
#define PKEY 0x60	//PUSHKEY


typedef struct{
    int s;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    char recvbuf[BUFSIZE];
}sock_t;

typedef struct{
    char parent[PARENT_MAX][BUFSIZE];
    char child[CHILD_MAX][BUFSIZE];
    int parent_flag;
    char topic[BUFSIZE];
    char data[BUFSIZE];
    char known_key[MAX_KEY][BUFSIZE];
}node_t;

typedef struct{
    char node_ip[BUFSIZE];
}list_t;

//グローバル変数
node_t node;	//自ノードに関する情報
list_t list[KNOWN_MAX];	//他ノードの情報
int first_connect;
int first_node;



void reset_node();
int start_p2p();
//node管理プロセス node間のコネクション管理
void node_connect_recv(sock_t *);
void node_connect_parent(sock_t *new_s);
void print_connect_node();
void node_send_other_node_info(sock_t *new_s);
void node_recieve_other_node_info(sock_t *new_s);
void print_node_list();
//query管理プロセス search query,keyの交換など
void query_key_push(sock_t *);
void query_key_receive(sock_t *);
void print_key();
//key管理プロセス 自ノードのkeyと他ノードのkeyの管理
//タスク管理 dataの転送,中継


int main(int argc,char *argv[]){


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

    //topic
    sprintf(node.topic,"room1/status");
    //data
    sprintf(node.data,"<temperature>23<humidity>40");

    for(i=0;i<MAX_KEY;i++){
	node.known_key[i][0]=0x00;
    }
    strcpy(node.known_key[0],node.topic);

    for(i=0;i<KNOWN_MAX;i++){
	list[i].node_ip[0]=0x00;
    }
    sprintf(list[0].node_ip,"192.168.1.10");
    sprintf(list[1].node_ip,"192.168.1.11");

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
    pthread_create(&worker,NULL,(void *)node_connect_parent,(void *)&new_s);
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
	new_s.addr = addr;
	new_s.senderinfo = senderinfo;
	strcpy(new_s.recvbuf,recvbuf);

	printf("[MAIN THREAD] recieve packet\n");
	printf("recvfrom: %s  port: %d  recv command: %x data: %s\n",senderstr,ntohs(senderinfo.sin_port),recvbuf[0],recvbuf);

	if(recvbuf[0] == CON){
	    pthread_create(&worker,NULL,(void *)node_connect_recv,(void *)&new_s);
	    printf("[MAIN THREAD] CREATE THREAD [%u]\n",worker);
	}else if(recvbuf[0] == ACK){
	    printf("[ALL]recv ACK\n");
	}else if(recvbuf[0] == PKEY){
	    pthread_create(&worker,NULL,(void *)query_key_receive,(void *)&new_s);
	    printf("[MAIN THREAD] CREATE THREAD [%u]\n",worker);
	}else{
	    printf("UNKNOWN COMMAND\n");
	}




	pthread_detach(worker);

    }

    close(sock);

    return 0;
}


void node_connect_recv(sock_t *new_s){

    pthread_t worker;
    int sock;
    sock_t new_s1;
    int i;
    int flag;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    char sendbuf;
    char senderstr[BUFSIZE];

    sock = new_s->s;
    addr = new_s->addr;
    senderinfo = new_s->senderinfo;

    inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderstr));
    //回線速度が自分の方が早いとき
    //まだ分岐しない
    
    //既に子になっているノードからのCONNECTに対する処理
    for(i=0;i<CHILD_MAX;i++){
	if(strcmp(node.child[i],senderstr) == 0){
	    sendbuf = CONREF;
	    sendto(sock,&sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));
	    //送信先の情報を出力
	    printf("[NODE]sendto: %s  port: %d  send command: %x\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);
	    return;

	}
    }

    //子のリンクに空きがあるか確認
    flag = -1;
    for(i=0;i<CHILD_MAX;i++){
	if(strcmp(node.child[i],"nothing") == 0 && flag != 0){
		sprintf(node.child[i],senderstr);
		flag = 0;
	}
    }

    if(flag == 0){

	//UDPで返信
	sendbuf=CONACK;
	sendto(sock,&sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));

	//送信先の情報を出力
	printf("[NODE]sendto: %s  port: %d  send command: %x\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);
    }else{	//子ノードがいっぱいで接続できない
	
	//UDPで返信
	sendbuf=CONREF;
	sendto(sock,&sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));

	//送信先の情報を出力
	printf("[NODE]sendto: %s  port: %d  send command: %x\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);

	//別のノード情報を送信
	new_s1.s = sock;
	new_s1.addr = addr;
	new_s1.senderinfo = senderinfo;

	printf("[NODE] MAX CHILD : send other node information\n");
	pthread_create(&worker,NULL,(void *)node_send_other_node_info,(void *)&new_s1);
	pthread_detach(worker);

    }

	print_connect_node();

}


void node_connect_parent(sock_t *new_s){


    pthread_t worker;
    int sock;
    sock_t new_s1;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    socklen_t senderinfolen;
    char target_ip[BUFSIZE];
    char sendbuf;
    char recvbuf[BUFSIZE];
    int i;
    int n;
    int flag;
    int count;
    //タイマ割り込みを発生されるための変数
    fd_set fds,readfds;
    int maxfd;
    int m;
    struct timeval tv;

    sock = socket(AF_INET,SOCK_DGRAM,0);
    count = 0;

    while(1){

	if(first_connect == -1){
	    strcpy(target_ip,TARGET);
	    first_connect = 0;
	}else if(list[0].node_ip != 0x00 && count < KNOWN_MAX){
	    strcpy(target_ip,list[count].node_ip);
	    count++;
	}else{
	    strcpy(target_ip,TARGET);
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

	    printf("[NODE]CONNECT MAX PARENT. CANCEL CONNECT\n");
	    node.parent_flag = 0;
	    return;
	}


	//sendtoでCONNECTを送信
	sendbuf=CON;
	n = sendto(sock,&sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&addr,sizeof(addr));
	printf("\n[NODE]send CONNECT [IP:%s]\n",target_ip);
	if(n < 1){
	    perror("sendto");
	    return;
	}


	//タイマ割り込みを発生されるための処理
	FD_ZERO(&readfds);
	FD_SET(sock,&readfds);
	tv.tv_sec = 10; //timeoutの時間
	tv.tv_usec = 0;
	maxfd = sock;

	memcpy(&fds,&readfds,sizeof(fd_set));
	printf("[NODE]wait reply from %s\n",target_ip);
	m = select(maxfd+1,&fds,NULL,NULL,&tv);

	if(m == 0){
	    printf("[NDOE]CONNECT PARENT session time out\n");
	}


	//返信待ち
	memset(recvbuf,0,sizeof(recvbuf));
	senderinfolen = sizeof(senderinfo);
	if(FD_ISSET(sock,&fds)){
	    recvfrom(sock,recvbuf,sizeof(recvbuf),0,(struct sockaddr *)&senderinfo,&senderinfolen);

	    new_s1.s = sock;
	    new_s1.senderinfo = senderinfo;
	    new_s1.addr = addr;

	    //CONNECT ACKの時は親のリストに追加
	    if(recvbuf[0] == CONACK){
		flag = -1;

		printf("\n[NODE]CONACK receive [IP:%s]\n",target_ip);
		for(i=0;i<PARENT_MAX;i++){
		    if(strcmp(node.parent[i],"nothing") == 0 && flag != 0){
			sprintf(node.parent[i],target_ip);

			sendbuf = ACK;
			n = sendto(sock,&sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));
			printf("\n[NODE}send ACK [IP:%s]\n",target_ip);
			flag = 0;

			printf("[QUERY SESSION START]\n");
			pthread_create(&worker,NULL,(void *)query_key_push,(void *)&new_s1);
			pthread_detach(worker);
		    }
		}
	    }else if(recvbuf[0] == CONREF){
		printf("[NODE]CONNECT REFUSE[IP:%s]  plz connect other node\n",target_ip);
		pthread_create(&worker,NULL,(void *)node_recieve_other_node_info,(void *)&new_s1);
		pthread_detach(worker);
	    }else{
		printf("[NODE]RECIEVE UNKNOWN COMMAND[IP:%s]",target_ip);
	    }
	}

	print_connect_node();
	sleep(10); //Dosにならないようsleep

    }

}

void print_connect_node(){

    int i;

    printf("PARENT: ");
    for(i=0;i<PARENT_MAX;i++){
	printf("%s ",node.parent[i]);
    }
    printf("CHILD: ");
    for(i=0;i<CHILD_MAX;i++){
	printf("%s ",node.child[i]);

    }
    printf("\n");

}

void node_send_other_node_info(sock_t *new_s){

    int sock;
    sock_t new_s1;
    int i;
    int n;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    char head;
    char sendbuf[BUFSIZE];
    char senderstr[BUFSIZE];

    sock = new_s->s;
    addr = new_s->addr;
    senderinfo = new_s->senderinfo;

    inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderstr));

    for(i=0;i<KNOWN_MAX;i++){
	if(list[i].node_ip[0]==0x00){
	    continue;
	}else{
	    head = OTHERINFO;
	    sprintf(sendbuf,"%c%s",head,list[i].node_ip);
	    sendto(sock,&sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));
	    if(n < 1){
		perror("sendto");
		return;
	    }
	    printf("[NODE]sendto: %s  port: %d  send command: %x\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);
	}
    }

    return;

}


void node_recieve_other_node_info(sock_t *new_s){


    int sock;
    sock_t new_s1;
    int i;
    int n;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    socklen_t senderinfolen;
    char head;
    char sendbuf[BUFSIZE];
    char senderstr[BUFSIZE];
    char recvbuf[BUFSIZE];
    char buf[BUFSIZE];

    sock = new_s->s;
    addr = new_s->addr;
    senderinfo = new_s->senderinfo;
    senderinfolen = sizeof(senderinfo);

    inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderstr));

    //whileで回してないのでノード情報は1つまでしか受け取れない（今のところ）
    recvfrom(sock,recvbuf,sizeof(recvbuf),0,(struct sockaddr *)&senderinfo,&senderinfolen);

    if(recvbuf[0] == OTHERINFO){

	for(i=1;i<sizeof(recvbuf);i++){
	    buf[i-1] = recvbuf[i];
	}

	for(i=0;i<KNOWN_MAX;i++){
	    if(list[i].node_ip[0] == 0x00){
		strcpy(list[i].node_ip,buf);
		break;
	    }else{
		continue;
	    }
	}

	head = ACK;
	n = sendto(sock,&head,sizeof(head),0,(struct sockaddr *)&addr,sizeof(addr));
	if(n < 1){
	    perror("sendto");
	    return;
	}

    print_node_list();

    }else{
	printf("[NODE]UNKNOWN COMMAND: recieve other node\n");
    }

    return;
}


void print_node_list(){


    int i;

    printf("*******PRINT NODE LIST*********\n");
    for(i=0;i<KNOWN_MAX;i++){
	if(list[i].node_ip[0] == 0x00){
	    continue;
	}else{
	    printf("NODE[%d] : %s\n",i,list[i].node_ip);
	}
    }

}

void query_key_push(sock_t *new_s){

    int sock;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    char senderstr[BUFSIZE];
    char *sendbuf;
    char head;
    char all_key[BUFSIZE];
    int i,n;

    sock = new_s->s;
    addr = new_s->addr;
    senderinfo = new_s->senderinfo;
    head = PKEY;

    sendbuf = (char *)malloc(BUFSIZE);
    for(i=0;i<MAX_KEY;i++){

	if(node.known_key[i][0] == 0x00){
	    continue;
	}else{

	    sprintf(sendbuf,"%c%s",head,node.known_key[i]);
	    n = sendto(sock,sendbuf,strlen(sendbuf)+1,0,(struct sockaddr *)&addr,sizeof(addr));
	    if(n < 1){
		perror("sendto");
		return;
	    }
	    inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderstr));
	    printf("[NODE]sendto: %s  port: %d  send command: %s\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);
	}
    }

    return;
    

}

void query_key_receive(sock_t *new_s){

    int sock;
    int i;
    int flag;
    struct sockaddr_in addr;
    struct sockaddr_in senderinfo;
    char sendbuf;
    char senderstr[BUFSIZE];
    char recvbuf[BUFSIZE];
    char buf[BUFSIZE];
    int n;

    sock = new_s->s;
    addr = new_s->addr;
    senderinfo = new_s->senderinfo;
    strcpy(recvbuf,new_s->recvbuf);

    inet_ntop(AF_INET,&senderinfo.sin_addr,senderstr,sizeof(senderstr));

    //headerの削除
    for(i=1;i<sizeof(recvbuf);i++){
	buf[i-1] = recvbuf[i];
    }

    for(i=0;i<MAX_KEY;i++){
	if(node.known_key[i][0] == 0x00){
	    strcpy(node.known_key[i],buf);
	    break;
	}else{
	    continue;
	}
    }

    sendbuf = ACK;
    n=sendto(sock,&sendbuf,sizeof(sendbuf),0,(struct sockaddr *)&senderinfo,sizeof(senderinfo));
    if(n < 1){
	perror("sendto");
	return;
    }
    printf("[NODE]sendto: %s  port: %d  send command: %x\n",senderstr,ntohs(senderinfo.sin_port),sendbuf);

    print_key();

}

void print_key(){

    int i;

    printf("*******PRINT KEY*********\n");
    for(i=0;i<MAX_KEY;i++){
	if(node.known_key[i][0] == 0x00){
	    continue;
	}else{
	    printf("KEY[%d] : %s\n",i,node.known_key[i]);
	}
    }

}




