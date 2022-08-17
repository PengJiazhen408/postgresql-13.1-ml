#ifndef _HTTP_TCPCLIENT_
#define _HTTP_TCPCLIENT_

#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define BUFFER_SIZE 1024

typedef struct _http_tcpclient{
	int 	socket;
	int 	remote_port;
	char 	remote_ip[16];
	struct sockaddr_in _addr; 
	int 	connected;
} http_tcpclient;


int http_tcpclient_create(http_tcpclient *pclient,const char *host, int port)
{
	struct hostent *he;

	if(pclient == NULL) return -1;

	memset(pclient,0,sizeof(http_tcpclient));

	if((he = gethostbyname(host))==NULL){
		return -2;
	}

	pclient->remote_port = port;
	strcpy(pclient->remote_ip,inet_ntoa( *((struct in_addr *)he->h_addr) ));

	pclient->_addr.sin_family = AF_INET;
	pclient->_addr.sin_port = htons(pclient->remote_port);
	pclient->_addr.sin_addr = *((struct in_addr *)he->h_addr);

	if((pclient->socket = socket(AF_INET,SOCK_STREAM,0))==-1){
		return -3;
	}

	return 0;
}

int http_tcpclient_conn(http_tcpclient *pclient)
{
	if(pclient->connected)
		return 1;

	if(connect(pclient->socket, (struct sockaddr *)&pclient->_addr,sizeof(struct sockaddr))==-1){
		return -1;
	}

	pclient->connected = 1;

	return 0;
}

int http_tcpclient_recv(http_tcpclient *pclient,char **lpbuff,int size)
{
	int recvnum=0,tmpres=0;
	char buff[BUFFER_SIZE];

	*lpbuff = NULL;

	while(recvnum < size || size==0){
		tmpres = recv(pclient->socket, buff,BUFFER_SIZE,0);
		if(tmpres <= 0)
			break;
		recvnum += tmpres;

		if(*lpbuff == NULL){
			*lpbuff = (char*)malloc(recvnum);
			if(*lpbuff == NULL)
				return -2;
		}else{
			*lpbuff = (char*)realloc(*lpbuff,recvnum);
			if(*lpbuff == NULL)
				return -2;
		}

		memcpy(*lpbuff+recvnum-tmpres,buff,tmpres);
	}

	return recvnum;
}

int http_tcpclient_send(http_tcpclient *pclient,char *buff,int size)
{
	int sent=0,tmpres=0;

	while(sent < size){
		tmpres = send(pclient->socket,buff+sent,size-sent,0);
		if(tmpres == -1){
			return -1;
		}
		sent += tmpres;
	}
	return sent;
}

int http_tcpclient_close(http_tcpclient *pclient)
{
	close(pclient->socket);
	pclient->connected = 0;
	return 0;
}


int http_post(http_tcpclient *pclient,char *page, char *request, char **response)
{
	char	*lpbuf, *ptmp;
	int	len;
	char	h_post[128], h_host[128], h_content_len[128], h_content_type[256];

	// const char *h_header="User-Agent: Mozilla/4.0\r\nCache-Control: no-cache\r\nAccept: */*\r\nAccept-Language: zh-cn\r\nConnection: Keep-Alive\r\n";
	const char *h_header="User-Agent: Mozilla/4.0\r\nCache-Control: no-cache\r\nAccept: */*\r\nConnection: Keep-Alive\r\n";

	printf("starting package\n");
	memset(h_post, 0, sizeof(h_post));
	sprintf(h_post, "POST %s HTTP/1.1\r\n", page);
	// memset(h_host, 0, sizeof(h_host));
	sprintf(h_host, "HOST: %s:%d\r\n",pclient->remote_ip, pclient->remote_port);
	memset(h_content_type, 0, sizeof(h_content_type));
	sprintf(h_content_type, "Content-Type: application/x-www-form-urlencoded\r\n");
	memset(h_content_len, 0, sizeof(h_content_len));
	sprintf(h_content_len,"Content-Length: %d\r\n", strlen(request));
	len = strlen(h_post)+strlen(h_host)+strlen(h_header)+strlen(h_content_len)+strlen(h_content_type)+strlen(request)+10;
	lpbuf = (char*)malloc(len);
	if(lpbuf==NULL){
		printf("Malloc error.\n");
		return -1;
	}
	strcpy(lpbuf,h_post);
	strcat(lpbuf,h_host);
	strcat(lpbuf,h_header);
	strcat(lpbuf,h_content_len);
	strcat(lpbuf,h_content_type);
	strcat(lpbuf,"\r\n");
	strcat(lpbuf,request);
	strcat(lpbuf,"\r\n");

	// printf("%s\n", lpbuf);

	//发送包
	if(http_tcpclient_send(pclient,lpbuf,len)<0){
		// printf("%s\n", lpbuf);
		free(lpbuf);
		return -2;
	}
	
	free(lpbuf);
	
	//接收包
	if(http_tcpclient_recv(pclient,&lpbuf,0) <= 0){
		return -3;
	}

	/*响应代码,|HTTP/1.1 200 OK|
	 *从第10个字符开始,共3位
	 * */
	memset(h_post,0,sizeof(h_post));
	strncpy(h_post,lpbuf+9,3);
	if(atoi(h_post)!=200){
		if(lpbuf) free(lpbuf);
		return atoi(h_post);
	}
	ptmp = (char*)strstr(lpbuf,"\r\n\r\n");
	if(ptmp == NULL){
		free(lpbuf);
		return -3;
	}
	ptmp += 4;/*跳过\r\n*/

	len = strlen(ptmp)+1;
	*response=(char*)malloc(len);
	if(*response == NULL){
		if(lpbuf) free(lpbuf);
		return -1;
	}
	memset(*response,0,len);
	memcpy(*response,ptmp,len-1);

	/*从头域找到内容长度,如果没有找到则不处理*/
	ptmp = (char*)strstr(lpbuf,"Content-Length:");
	if(ptmp != NULL){
		char *ptmp2;
		ptmp += 15;
		ptmp2 = (char*)strstr(ptmp,"\r\n");
		if(ptmp2 != NULL){
			memset(h_post,0,sizeof(h_post));
			strncpy(h_post,ptmp,ptmp2-ptmp);
			if(atoi(h_post)<len)
				(*response)[atoi(h_post)] = '\0';
		}
	}

	if(lpbuf) free(lpbuf);

	return 0;
}


#endif

