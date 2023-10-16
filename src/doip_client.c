#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>

int socket_init(void)
{
	int sock = -1;
	int res = -1;
	struct sockaddr_in s_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(4320),
		.sin_addr.s_addr = inet_addr("127.0.0.1")
	};
	struct sockaddr_in m_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(4321),
		.sin_addr.s_addr = inet_addr("127.0.0.1")
	};
	int reuseaddr = 1;


	do
	{
		if((sock = socket(PF_INET, SOCK_STREAM, 0)) == - 1)
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
		else if(connect(sock, (struct sockaddr *)&s_addr, sizeof(s_addr)) == -1)
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
			/* null */
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
void sign_handler(int signum)
{
}
int main(int argc, char *argv[])
{
	int res = -1;
	int sock = -1;
	int i = 0;
	ssize_t size = 0;
	char buf[1024] = {0};
	int state = 0;

	signal(SIGPIPE, sign_handler);
	for(state = 0; state < 2;)
	{
		switch(state)
		{
			case 0:
				if((sock = socket_init()) == -1)
				{
					state = 2;
				}
				else
				{
					state = 1;
				}
				break;
			case 1:
				for(i = 0; i < argc - 1; i++)
				{
					buf[i] = (uint8_t)strtol(argv[i + 1], NULL, 16);
				}
				while(i > 0)
				{
					
					size = send(sock, buf, i, 0);
					if(size == -1)
					{
						perror("c:send");
						break;
					}
					memset(buf, 0, sizeof(buf));
					recv(sock, buf, sizeof(buf), 0);
					//printf("doip_cli result ");
					for(int i = 0; i < 20; i++)
					{
						printf("%02X ", (uint8_t)buf[i]);
					}
					printf("\n");
					break;
				}
				close(sock);
				state = 2;
			default:
				break;
		}
	}

	return res;
}
