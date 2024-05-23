#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <iostream>
#include "MyHttp.h"

using namespace std;

#define MAXSIZE 2048

//用于检查返回值
void checkError(int status, char* str) {
	if (status == -1) {
		perror(str);
		exit(1);
	}
}

//初始化监听
int init_listen_fd(int port,int epfd)
{
	//创建监听的套接字 lfd
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1) {
		perror("socket error");
		exit(1);
	}

	//创建服务器地址结构  IP + Port
	struct sockaddr_in serv_addr;
	
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//端口复用
	int opt = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	//给lfd绑定地址结构
	int ret = bind(lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	checkError(ret,"bind error");  //用于检查返回值

	//设置监听上限
	ret = listen(lfd, 128);
	checkError(ret, "listen error");

	//lfd 添加到epoll树上
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = lfd;

	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	checkError(ret, "epoll_ctl add lfd error");

	return lfd;
}

//连接客户端
void do_accept(int lfd,int epfd)
{
	struct sockaddr_in clt_addr;
	socklen_t clt_addr_len = sizeof(clt_addr);

	int cfd = accept(lfd, (struct sockaddr*)&clt_addr, &clt_addr_len);
	checkError(cfd, "accept error");

	//打印客户端IP+Port
	char client_ip[64] = { 0 };

	cout << "New client IP:" << inet_ntop(AF_INET, &clt_addr.sin_addr.s_addr, client_ip, sizeof(client_ip))
		<< ",Port: " << ntohs(clt_addr.sin_port)
		<< " cfd = " << cfd << endl;

	//设置cfd非阻塞  
	int flag = fcntl(cfd, F_GETFL);  //改变已打开的文件性质
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);

	//将新节点cfd挂到epoll监听树上
	struct epoll_event ev;
	ev.data.fd = cfd;

	//边缘非阻塞模式
	ev.events = EPOLLIN | EPOLLET;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	checkError(ret, "epoll_ctl add cfd error");

}

void do_read(int cfd,int epfd)
{

}


//epoll监听   create  ctl  wait
void epoll_run(int port)
{
	int i = 0;
	struct epoll_event all_events[MAXSIZE];

	//创建一个epoll监听树根
	int epfd = epoll_create(MAXSIZE);
	if (epfd == -1) {
		perror("epoll_create error");
		exit(1);
	}

	//创建lfd,并添加至监听树
	int lfd = init_listen_fd(port, epfd);

	while (1)
	{
		//监听节点对应事件   -1好像是阻塞
		int ret = epoll_wait(epfd, all_events, MAXSIZE, -1);
		if (ret == -1) {
			//这需要分类讨论
			if (ret == EAGAIN) {
				continue;
			}
			else if (ret == EINTR) {
				continue;
			}
			else {
				perror("epoll_wait error");
				exit(1);
			}
		} // end if

		for (int i = 0; i < ret; ++i) {
			//只处理读事件， 其他事件默认不处理
			struct epoll_event* pev = &all_events[i];

			//不是读事件
			if (!(pev->events & EPOLLIN)) {
				continue;
			}
			if (pev->data.fd == lfd) {  //接受连接请求
				do_accept(lfd, epfd);
			}
			else { //读数据
				do_read(pev->data.fd, epfd);
			}
		}

	}

}


int main(int argc, char* argv[]) {

	//命令行参数获取   端口 和 server提供的目录
	if (argc < 3)
	{
		cout << "./server port path" << endl;
	}
	cout << "hello world" << endl;
	//获取用户输入的端口
	int port = atoi(argv[1]);

	//改变进程工作目录
	int ret = chdir(argv[2]);
	if (ret != 0) {
		perror("chdir error");
		exit(1);
	}

	//启动 epoll监听
	epoll_run(port);

	return 0;
}



















