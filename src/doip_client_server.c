#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

typedef unsigned char UC;
typedef unsigned short US;
typedef unsigned int UI;

#define R_US_DOIP_METER_PROTOCOL                        ((UC)0x02)                      /* Protocol Version  */
#define R_US_DOIP_METER_INVERSE                         ((UC)0xFD)                      /* Protocol Version  */
#define R_UI_DOIP_TCPRECV_BUF                           ((UI)1500)                      /* max data */
#define TMP_BUF_SIZE 128
/*--------------------------*/
/*      struct define       */
/*--------------------------*/
typedef struct
{
	UC      ucProtoVer;             /*  1byte       Protocol Version */
	UC      ucInverProtoVer;        /*  1byte       Inverse Protocol Version */
	US      usPaylaodType;          /*  2byte       Paylaod Type */
	UI      uiPaylaodLeng;          /*  4byte       Payload Length */
}ST_DOIP_DATAHEAD;

typedef struct
{
	US      usSourceAddr;           /*  2byte       Source Address */
	US      usTargetAdrr;           /*  2byte       Target address */
}ST_DOIP_DATA_8001;

/* 0x0005 data */
enum Doip0005Data
{
	R_UC_DOIPD_ROUTE_REQ_DEFAULT=(UC)(0x00),	/* Default */
	R_UC_DOIPD_ROUTE_REQ_DIAG_REGULATION,   	/* Diagnostic communication required by regulation */
	R_UC_DOIPD_ROUTE_REQ_VM_SPECIFIC=(UC)(0xE0)	/* Central security */
};

typedef struct
{
	US      usSourceAddr;           /*  2byte       Source Address */
}ST_DOIP_DATA_0008;

#pragma pack(1)
typedef struct
{
	US      usSourceAddr;           /*  2byte       Source Address */
	UC      ucActType;              /*  1byte       Activation Type */
	UI      uiReserved;             /*  4byte       Reserved */
	UI      uiVMspecific;           /*  4byte       VMspecific */
}ST_DOIP_DATA_0005;
#pragma pack(0)

char doip_server_ip[17] = {0};
char doip_client_ip[17] = {0};
uint16_t doip_source_addr = 0;
uint16_t doip_target_addr = 0;

int get_doip_config(void)
{
	int result = -1;
	FILE *fp = fopen("doip.conf", "r");
	char buf[128 + 1] = {0};
	char *pbuf = &buf[0];
	ssize_t len = 0;
	int line = 0;
	size_t line_len = 0;

	if(!fp)
	{
		fprintf(stderr, "doip.conf not exist.\n");
	}
	else
	{
		do
		{
			line_len = sizeof(buf) - 1;
			memset(&buf[0], 0, sizeof(buf));
			if((len = getline(&pbuf, &line_len, fp)) == -1)
			{
				break;
			}
			else if(buf[len - 1] != '\n')
			{
				fprintf(stderr, "line %d,%s conf error\n", line, &buf[0]);
			}
			else if(buf[0] == '#')
			{
				/* skip this line */
			}
			else
			{
				if(!strncmp(&buf[0], "doip_server_ip=", 15))
				{
					for(int i = 0; i < 17; i++)
					{
						if(buf[15 + i] == '\n')
						{
							break;
						}
						else
						{
							doip_server_ip[i] = buf[15 + i];
						}
					}
				}
				else if(!strncmp(&buf[0], "doip_client_ip=", 15))
				{
					for(int i = 0; i < 17; i++)
					{
						if(buf[15 + i] == '\n')
						{
							break;
						}
						else
						{
							doip_client_ip[i] = buf[15 + i];
						}
					}
				}
				else if(!strncmp(&buf[0], "doip_source_addr=", 17))
				{
					doip_source_addr = strtol(&buf[17], NULL, 0);
				}
				else if(!strncmp(&buf[0], "doip_target_addr=", 17))
				{
					doip_target_addr = strtol(&buf[17], NULL, 0);
				}
				else
				{
					/* keep */
				}
			}
			line++;
		}
		while(1);
		fclose(fp);
	}
	if(doip_server_ip[0] == '\0')
	{
		fprintf(stderr, "doip server ip not configured\n");
	}
	else if(doip_client_ip[0] == '\0')
	{
		fprintf(stderr, "doip client ip not configured\n");
	}
	else if(doip_source_addr == 0)
	{
		fprintf(stderr, "doip_source_addr not configured\n");
	}
	else if(doip_target_addr == 0)
	{
		fprintf(stderr, "doip_target_addr not configured\n");
	}
	else
	{
		printf("server ip=%s\n", &doip_server_ip[0]);
		printf("client ip=%s\n", &doip_client_ip[0]);
		printf("client address=0x%04X\n", doip_source_addr);
		printf("server address=0x%04X\n", doip_target_addr);
		result = 0;
	}
	return result;
}

#define DOIP_RECV_STATE_RECV_HEAD    0u
#define DOIP_RECV_STATE_RECV_BODY    1u
#define DOIP_RECV_STATE_RECV_DONE    2u
#define DOIP_RECV_STATE_RECV_ERROR   255u

ssize_t doip_recv(int sockfd, void *buf, size_t len, int flags)
{
	int wk_siRecvState = DOIP_RECV_STATE_RECV_HEAD;
	int wk_siRecvLeng = 0;
	int wk_siTcpClientfd = sockfd;
	int wk_siNum = 0;
	int wk_siRcvPos = 0;
	char *wk_ucpDoipTcpRecvBuf = buf;
	uint32_t wk_uiDoipFrameLen = 0;
	ssize_t wk_slRet = -1;
	fd_set wk_stReadfds = {0};

	(void)flags;
	do
	{
		FD_ZERO(&wk_stReadfds);
		FD_SET(sockfd, &wk_stReadfds);
		if((wk_siNum = select(sockfd + 1, &wk_stReadfds, NULL, NULL, NULL)) == -1)
		{
			fprintf(stderr, "%s,%d,%s\n", __func__, __LINE__, strerror(errno));
			wk_siRecvState = 255;
		}
		else
		{
			switch(wk_siRecvState)
			{
				case DOIP_RECV_STATE_RECV_HEAD:
					wk_siRecvLeng = recv(wk_siTcpClientfd, wk_ucpDoipTcpRecvBuf, 8 - wk_siRcvPos, 0);
					if(((wk_siRecvLeng == -1) && (errno != EINTR)) || (wk_siRecvLeng == 0))
					{
						fprintf(stderr, "%s,%d,%s\n", __func__, __LINE__, strerror(errno));
						wk_siRecvState = DOIP_RECV_STATE_RECV_ERROR;
					}
					else if((wk_siRcvPos += wk_siRecvLeng) != 8)
					{
						/* null */
					}
					else if((wk_uiDoipFrameLen = ntohl(*(uint32_t *)(wk_ucpDoipTcpRecvBuf + 4)) + 8) > R_UI_DOIP_TCPRECV_BUF)
					{
						fprintf(stderr, "%s,%d,bad pkg len %u\n", __func__, __LINE__, wk_uiDoipFrameLen);
						wk_siRecvState = DOIP_RECV_STATE_RECV_ERROR;
					}
					else
					{
						wk_siRecvState = DOIP_RECV_STATE_RECV_BODY;
					}
					break;
				case DOIP_RECV_STATE_RECV_BODY:
					wk_siRecvLeng = recv(wk_siTcpClientfd, wk_ucpDoipTcpRecvBuf + wk_siRcvPos, wk_uiDoipFrameLen - wk_siRcvPos, 0);
					if(((wk_siRecvLeng == -1) && (errno != EINTR)) || (wk_siRecvLeng == 0))
					{
						fprintf(stderr, "%s,%d,%s\n", __func__, __LINE__, strerror(errno));
						wk_siRecvState = DOIP_RECV_STATE_RECV_ERROR;
					}
					else if(wk_siRecvLeng == -1)
					{
						/* null */
					}
					else if((wk_siRcvPos += wk_siRecvLeng) < wk_uiDoipFrameLen)
					{
						/* null */
					}
					else
					{
						wk_siRecvState = DOIP_RECV_STATE_RECV_DONE;
					}
					break;
				default:
					wk_siRecvState = DOIP_RECV_STATE_RECV_ERROR;
					break;
			}
		}
	}
	while((wk_siRecvState != DOIP_RECV_STATE_RECV_ERROR) && (wk_siRecvState != DOIP_RECV_STATE_RECV_DONE));
	if(wk_siRecvState == DOIP_RECV_STATE_RECV_ERROR)
	{
		printf("close sock\n");
		close(wk_siTcpClientfd);
		exit(-1);
		wk_slRet = -1;
	}
	else
	{
		wk_slRet = wk_uiDoipFrameLen;
	}
	return wk_slRet;
}
ssize_t safe_send(int sockfd, const void *buf, size_t len, int flags)
{
	ssize_t sent = 0;
	ssize_t s = 0;

	do
	{
		s = send(sockfd, buf + sent, len - sent, flags);
		if(sent <= 0)
		{
			break;
		}
		else
		{
			sent += s;
		}
	}
	while(sent < len);
	if(sent < len)
	{
		sent = s;
	}
	else
	{
		/* null */
	}
	return sent;
}
ssize_t send_alive_response(int sockfd)
{
	char buf[10] = {0};
	ST_DOIP_DATAHEAD *head = (ST_DOIP_DATAHEAD *)&buf[0];
	ST_DOIP_DATA_0008 *data = (ST_DOIP_DATA_0008 *)&head[1];

	head->ucProtoVer = R_US_DOIP_METER_PROTOCOL;
	head->ucInverProtoVer = R_US_DOIP_METER_INVERSE;
	head->usPaylaodType = htons(0x0008);
	head->uiPaylaodLeng = htonl(2);
	data->usSourceAddr = htons(doip_source_addr);
	return safe_send(sockfd, &buf[0], sizeof(buf), 0);
}
void *alive_thread(void *arg)
{
	int sockfd = (int)(long)arg;

	printf("%s enter\n", __func__);
	while(1)
	{
		sleep(1);
		if(send_alive_response(sockfd) != -1)
		{
			/* null */
		}
		else
		{
			perror("alive_thread exit");
			break;
		}
	}
	close(sockfd);
	printf("%s exit\n", __func__);
	pthread_exit(NULL);
}
int socket_init(void)
{
	int sock = -1;
	struct sockaddr_in m_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(4320),
		.sin_addr.s_addr = inet_addr("127.0.0.1")
	};
	int reuseaddr = 1;

	do
	{
		if((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		{
			perror("socket");
		}
		else if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, 4) == -1)
		{
			perror("setsockopt");
		}
		else if(bind(sock, (struct sockaddr *)&m_addr, sizeof(m_addr)) == -1)
		{
			perror("bind");
		}
		else if(listen(sock, 0) == -1)
		{
			perror("listen");
		}
		else
		{
			break;
		}
		/* on error */
		if(sock == -1)
		{
			break;
		}
		else
		{
			close(sock);
			sock = -1;
		}
	}
	while(0);

	return sock;
}
int doip_socket_init(void)
{	
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	char ok = 1;
	struct sockaddr_in c_addr = {
		.sin_family        = AF_INET,
		.sin_port          = htons(13400),
		.sin_addr.s_addr   = inet_addr("")
	};
	struct sockaddr_in bind_addr = {
		.sin_family        = AF_INET,
		.sin_port          = htons(49153),
		.sin_addr.s_addr   = inet_addr("")
	};
	struct timeval timeout = {
		.tv_sec = 1,         /* seconds */
		.tv_usec = 1000        /* microseconds */
	};

	int wk_siTtl = 3;
	int value = 1;
	int client = -1;

	do
	{
		if(get_doip_config() == -1)
		{
			ok = 0;
		}
		else
		{
			c_addr.sin_addr.s_addr = inet_addr(doip_server_ip);
			bind_addr.sin_addr.s_addr = inet_addr(doip_client_ip);
		}
		printf("connecting to doip server ...\n");
		if(ok == 0)
		{
			/* keep */
		}
		else if(setsockopt(sock, IPPROTO_IP, IP_TTL, (char *)&wk_siTtl, 4) == -1)
		{
			perror("setsockopt");
		}
		else if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &value, 4) == -1)
		{
			perror("setsockopt");
		}
		else if(bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == -1)
		{
			perror("bind");
		}
		else if((client = connect(sock, (struct sockaddr *)&c_addr, sizeof(c_addr))) == -1)
		{
			perror("connect");
		}
		else
		{
			break;
		}
		/* on error */
		if(sock == -1)
		{
			break;
		}
		else
		{
			close(sock);
			sock = -1;
		}
	}
	while(0);

	return sock;
}
void safe_send_data0005(int sock)
{
	ssize_t len = 0;
	char *buf = calloc(1, TMP_BUF_SIZE);
	uint16_t type = 0;
	ST_DOIP_DATAHEAD *doipHead = (ST_DOIP_DATAHEAD *)&buf[0];
	ST_DOIP_DATA_0005 *data0005 = (void *)doipHead + sizeof(*doipHead);

	/* Type: Routing activation request (0x0005)*/
	doipHead->ucProtoVer = R_US_DOIP_METER_PROTOCOL;
	doipHead->ucInverProtoVer = R_US_DOIP_METER_INVERSE;
	doipHead->usPaylaodType = htons(0x0005);
	doipHead->uiPaylaodLeng = htonl(sizeof(ST_DOIP_DATA_0005));
	data0005->usSourceAddr = htons(doip_source_addr),
		data0005->ucActType = R_UC_DOIPD_ROUTE_REQ_DEFAULT;
	data0005->uiReserved = 0;
	data0005->uiVMspecific = 0;

	safe_send(sock, doipHead, sizeof(ST_DOIP_DATAHEAD) + sizeof(ST_DOIP_DATA_0005), 0);
	do
	{
		memset(buf, 0, TMP_BUF_SIZE);
		len = doip_recv(sock, buf, TMP_BUF_SIZE, 0);
		type = ntohs(*(uint16_t *)&buf[2]);
		if(type == 0x0007)
		{
			send_alive_response(sock);
		}
		else if(type != 0x0006)
		{
			printf("%d,type error:%04x\n", __LINE__, type);
		}
		else
		{
			//res = 0;
			printf("Type: Routing activation response (0x0006) ok\n");
			break;
		}
	}
	while(len != -1);

	free(buf);
}
int tcp_work(int sock, char data[], int32_t size)
{
	int res = -1;
	char *buf = NULL;	
	uint16_t type = 0;
	ssize_t len = 0;

	if(size <= 0)
	{
		printf("bad\n");
	}
	else if((buf = calloc(1, 8192)) == NULL)
	{
		perror("calloc");
	}
	else
	{
		memset(buf, 0, 8192);
		ST_DOIP_DATAHEAD *doipHead = (ST_DOIP_DATAHEAD *)&buf[0];
		ST_DOIP_DATA_8001 *data8001 = (ST_DOIP_DATA_8001 *)&doipHead[1];
		char *wk_ucpUdsData = NULL;
		// safe_send 8001
		doipHead->ucProtoVer = R_US_DOIP_METER_PROTOCOL;
		doipHead->ucInverProtoVer = R_US_DOIP_METER_INVERSE;
		doipHead->usPaylaodType = htons(0x8001);
		doipHead->uiPaylaodLeng = htonl(sizeof(ST_DOIP_DATA_8001) + size);
		data8001->usSourceAddr = htons(doip_source_addr);
		data8001->usTargetAdrr = htons(doip_target_addr);
		wk_ucpUdsData = (char *)&data8001[1];
		//ucUDSServiceRequest:byte0-7	=VH_ulFotaCheckCode
		//ucUDSServiceRequest:byte7-15	=wk_siRecvLeng
		//ucUDSServiceRequest:byte16-	=wk_ucpUdsData
		memcpy(wk_ucpUdsData, data, size);
		if(safe_send(sock, &buf[0], (char *)&wk_ucpUdsData[size] - (char *)&buf[0], MSG_NOSIGNAL) == -1)
		{
			perror("doip safe_send");
		}
		do
		{
			len = doip_recv(sock, buf, TMP_BUF_SIZE, 0);
			type = ntohs(*(uint16_t *)&buf[2]);
			if(type == 0x0007)
			{
				send_alive_response(sock);
			}
			else if(type != 0x8002 && type != 0x8003)
			{
				printf("%d,type error:%04x\n", __LINE__, type);
			}
			else
			{
				res = 0;
				break;
			}
		}
		while(len != -1);
		free(buf);
	}
	return res;
}


int main(int argc, char *argv[])
{
	int sock = -1;
	int client = -1;
	ssize_t len = 0;
	uint16_t type = 0;
	struct sockaddr_in c_addr = {0};
	socklen_t slen = sizeof(c_addr);
	ssize_t res = 0;
	uint8_t buf[8192] = {0};
	int state = 0;
	int doip_sock = -1;
	int doip_sock_reconnect = 0;
	FILE *filp = NULL;
	uint32_t dl_remained_size = 0;
	uint32_t dl_processed_size = 0;
	int32_t read_size = 0;
	uint8_t gu8_block_seq_cnt = 0;
	pthread_t tid = 0;
	time_t t1 = {0};
	time_t t2 = {0};
	int filter = 0;
	int print_len = 0;

	signal(SIGPIPE, SIG_IGN);
	while(1)
	{
		switch(state)
		{
			case 0:
				doip_sock = doip_socket_init();
				if(doip_sock == -1)
				{
					usleep(1000*1000*3);
				}
				else
				{
					if(doip_sock_reconnect)
					{
						state = 3;
						doip_sock_reconnect = 0;
						printf("reconnect ok\n");
					}
					else
					{
						state = 1;
					}
					pthread_create(&tid, NULL, alive_thread, (void *)(long)doip_sock);
					safe_send_data0005(doip_sock);
				}
				break;
			case 1:
				if((sock = socket_init()) == -1)
				{
					usleep(1000*1000*3);
				}
				else
				{
					state = 2;
				}
				break;
			case 2:
				if((client = accept(sock, (struct sockaddr *)&c_addr, &slen)) == -1)
				{
					perror("accept");
					usleep(1000*1000*2);
				}
				else
				{
					state = 3;
					printf("%s:%d connected\n", inet_ntoa(c_addr.sin_addr), ntohs(c_addr.sin_port));
				}
				break;
			case 3:
				res = recv(client, buf, sizeof(buf), 0);
				if(res == -1)
				{
					perror("recv");
				}
				else if(res == 0)
				{
					/* keep */
				}
				else if(buf[0] == 0x45 && buf[1] == 0x58 && buf[2] == 0x49 && buf[3] == 0x54) // 0x45 0x58 0x49 0x54 #EXIT
				{
					printf("EXIT\n");
					close(client);
					close(doip_sock);
					exit(0);
				}
				else if(buf[0] == 0x75 && buf[1] == 0x70 && buf[2] == 0x6c && buf[3] == 0x6f && buf[4] == 0x61 && buf[5] == 0x64) // 75 70 6c 6f 61 64 #upload
				{
					printf("ready upload file to meter\n");
					if((filp = fopen("dl_gp_package.bin", "r")) == NULL)
						//if((filp = fopen("10MB.bin", "r")) == NULL)
					{
						perror("fopen");
					}
					else
					{	fseek(filp, 0L, SEEK_END);
						dl_remained_size = ftell(filp);
						dl_processed_size = 0;
						fseek(filp, 0L, SEEK_SET);
						printf("file size=%d\n", dl_remained_size);
						//request download
						//byte3-6=u32_address=gu32_dl_processed_size
						//#byte7-10=u32_remaind=gu32_dl_remained_size
						//for(int i = 0; i < 10; i++)
						buf[0] = 0x34;
						buf[1] = 0x00;
						buf[2] = 0x44;
						*((uint32_t *)&buf[3]) = htonl(dl_processed_size);
						*((uint32_t *)&buf[7]) = htonl(dl_remained_size);
						tcp_work(doip_sock, buf, 11);
						do
						{
							len = doip_recv(doip_sock, buf, TMP_BUF_SIZE, 0);
							type = ntohs(*(uint16_t *)&buf[2]);
							if(type == 0x0007)
							{
								send_alive_response(doip_sock);
							}
							else if(type != 0x8001)
							{
								printf("%d,type error:%04x\n", __LINE__, type);
							}
							else
							{
								res = 0;
								break;
							}
						}
						while(len != -1);

						t1 = time(NULL);
						while(dl_remained_size > 0)
						{
							//transfer_data
							buf[0] = 0x36;
							buf[1] = ++gu8_block_seq_cnt;
							read_size = fread(&buf[2], 1, 4104 - 14, filp);
							tcp_work(doip_sock, buf, 2 + read_size);
							do
							{
								len = doip_recv(doip_sock, buf, TMP_BUF_SIZE, 0);
								type = ntohs(*(uint16_t *)&buf[2]);
								if(type == 0x0007)
								{
									send_alive_response(doip_sock);
								}
								else if(type != 0x8001)
								{
									printf("%d,type error:%04x\n", __LINE__, type);
								}
								else
								{
									if((buf[12] != 0x76) || (buf[13] != gu8_block_seq_cnt))
									{
										printf("this pkg %02x,%02x,%02x may wrong\n",buf[12], buf[13],gu8_block_seq_cnt);
									}
									else
									{
										res = 0;
										break;
									}
								}
							}
							while(len != -1);


							dl_remained_size -= read_size;
							dl_processed_size += read_size;

							if(filter < 5)
							{
								filter++;
							}
							else
							{
								filter = 0;
								t2 = time(NULL);
								if(t2 <= t1)
								{
									/* keep */
								}
								else
								{
									if(print_len <= 0)
									{
										/* keep */
									}
									else
									{
										printf("\r");
										for(int i = 0; i < print_len; i++)
										{
											printf(" ");
										}
									}
									print_len = printf("\rdl_remained_size/dl_processed_size=%u,%u,progress:%f%,speed=%ldKByte/s", \
											dl_remained_size, dl_processed_size, \
											100.0*dl_processed_size/(dl_remained_size+dl_processed_size), \
											dl_processed_size / (t2 - t1) / 1024);
									printf("\e[?25l");
									fflush(stdout);
								}
							}
						}
						printf("\n\e[?25h");
						//transfer_data_exit
						buf[0] = 0x37;
						tcp_work(doip_sock, buf, 1);
						do
						{
							len = doip_recv(doip_sock, buf, TMP_BUF_SIZE, 0);
							type = ntohs(*(uint16_t *)&buf[2]);
							if(type == 0x0007)
							{
								send_alive_response(doip_sock);
							}
							else if(type != 0x8001)
							{
								printf("%d,type error:%04x\n", __LINE__, type);
							}
							else
							{
								res = 0;
								break;
							}
						}
						while(len != -1);

						safe_send(client, buf, len, 0);
						fclose(filp);
					}
					printf("upload file to meter end\n");
				}
				else
				{
					if(tcp_work(doip_sock, buf, res) == -1)
					{
						doip_sock_reconnect = 1;
						printf("reconnect meter doip\n");
						state = 0;
						memset(buf, 0, sizeof(buf));
					}
					else
					{
						do
						{
							len = doip_recv(doip_sock, buf, TMP_BUF_SIZE, 0);
							type = ntohs(*(uint16_t *)&buf[2]);
							if(type == 0x0007)
							{
								send_alive_response(doip_sock);
							}
							else if(type != 0x8001)
							{
								printf("%d,type error:%04x\n", __LINE__, type);
							}
							else
							{
								res = 0;
								break;
							}
						}
						while(len != -1);
					}
					safe_send(client, buf, len, 0);
				}
				/* end of doip_cli */
				close(client);
				if(doip_sock_reconnect)
				{
					close(doip_sock);
				}
				else
				{
					/* wait next doip_cli connect */
					state = 2;
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

