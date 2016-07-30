#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <string.h>

static int startup(const char *_ip, int _port)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("socket");
		exit(2);
	}

	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(_port);
	local.sin_addr.s_addr = inet_addr(_ip);

	if(bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0)
	{
		perror("bind");
		exit(3);
	}

	if(listen(sock, 5) < 0)
	{
		perror("listen");
		exit(4);
	}
	return sock;
}

static int set_noblock(int sock)
{
	int fl = fcntl(sock, F_GETFL);
	return fcntl(sock, F_SETFL, fl|O_NONBLOCK);
}

static void usage(const char *proc)
{
	printf("Usage: %s [ip] [port]\n", proc);
}

int main(int argc, char *argv[])
{
	if( argc != 3)
	{
		usage(argv[0]);
		exit(1);
	}
	int listen_sock = startup(argv[1], atoi(argv[2])); // create listen socket

	int epfd = epoll_create(256);
	if(epfd < 0)
	{
		perror("epoll_create");
		exit(5);
	}

	struct epoll_event _ev;
	_ev.events = EPOLLIN;
	_ev.data.fd = listen_sock;

	epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &_ev);

	struct epoll_event _ready_ev[128];
	int _ready_evs = 128;
	int _timeout = -1; // block

	int nums = 0;
	int done = 0;
	while(!done)
	{
		switch( (nums = epoll_wait(epfd, _ready_ev,\
						_ready_evs, _timeout)))
		{
			case 0:
				printf("timeout...\n");
				break;
			case -1:
				perror("epo;;_wait");
				break;
			default:
				{
					int i = 0;
					for(; i < nums; ++i)
					{
						int _fd = _ready_ev[i].data.fd;
						if(_fd == listen_sock && \
								_ready_ev[i].events & EPOLLIN)
						{
							//get a new link...
							struct sockaddr_in peer;
							socklen_t len = sizeof(peer);
							int new_sock = accept(listen_sock, (struct sockaddr*)&peer, &len);
							if(new_sock > 0)
							{
								printf("client info, socket:%s:%d\n", inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
								_ev.events = EPOLLIN | EPOLLET;//ET
								_ev.data.fd = new_sock;

								set_noblock(new_sock);

								epoll_ctl(epfd, EPOLL_CTL_ADD,\
										new_sock, &_ev);
							}
							else
							{
								if(_ready_ev[i].events & EPOLLIN)
								{
									char buf[102400];
									memset(buf, '\0', sizeof(buf));
									//read/write
									ssize_t _s = recv(_fd, buf,\
											sizeof(buf)-1, 0); //while need recv done...
									if( _s > 0 )
									{
										printf("client# %s\n", buf);
										_ev.events = EPOLLOUT|EPOLLET;
										_ev.data.fd = _fd;
										epoll_ctl(epfd, EPOLL_CTL_MOD, \
												_fd, &_ev);
									}
									else if( _s == 0 )
									{
										printf("client close...\n");
										epoll_ctl(epfd, EPOLL_CTL_DEL,\
												_fd, NULL);
											close(_fd);
							
									}
									else
									{
										perror("recv");
									}
								}
								else if(_ready_ev[i].events &\
										EPOLLOUT)
								{
									const char *msg = "HTTP/1.1 200 OK\r\n\r\n<h1>hello world  = =# </h1>\r\n";
									send(_fd, msg, strlen(msg), 0);
									epoll_ctl(epfd,EPOLL_CTL_DEL, _fd, NULL);
									close(_fd);
								}
							}
						}
					}
					break;
				}
		}
	}
}

