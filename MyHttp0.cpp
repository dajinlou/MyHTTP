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

//���ڼ�鷵��ֵ
void checkError(int status, char* str) {
	if (status == -1) {
		perror(str);
		exit(1);
	}
}

//��ʼ������
int init_listen_fd(int port,int epfd)
{
	//�����������׽��� lfd
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1) {
		perror("socket error");
		exit(1);
	}

	//������������ַ�ṹ  IP + Port
	struct sockaddr_in serv_addr;
	
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//�˿ڸ���
	int opt = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	//��lfd�󶨵�ַ�ṹ
	int ret = bind(lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	checkError(ret,"bind error");  //���ڼ�鷵��ֵ

	//���ü�������
	ret = listen(lfd, 128);
	checkError(ret, "listen error");

	//lfd ��ӵ�epoll����
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = lfd;

	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	checkError(ret, "epoll_ctl add lfd error");

	return lfd;
}

//���ӿͻ���
void do_accept(int lfd,int epfd)
{
	struct sockaddr_in clt_addr;
	socklen_t clt_addr_len = sizeof(clt_addr);

	int cfd = accept(lfd, (struct sockaddr*)&clt_addr, &clt_addr_len);
	checkError(cfd, "accept error");

	//��ӡ�ͻ���IP+Port
	char client_ip[64] = { 0 };

	cout << "New client IP:" << inet_ntop(AF_INET, &clt_addr.sin_addr.s_addr, client_ip, sizeof(client_ip))
		<< ",Port: " << ntohs(clt_addr.sin_port)
		<< " cfd = " << cfd << endl;

	//����cfd������  
	int flag = fcntl(cfd, F_GETFL);  //�ı��Ѵ򿪵��ļ�����
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);

	//���½ڵ�cfd�ҵ�epoll��������
	struct epoll_event ev;
	ev.data.fd = cfd;

	//��Ե������ģʽ
	ev.events = EPOLLIN | EPOLLET;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	checkError(ret, "epoll_ctl add cfd error");

}

void do_read(int cfd,int epfd)
{

}


//epoll����   create  ctl  wait
void epoll_run(int port)
{
	int i = 0;
	struct epoll_event all_events[MAXSIZE];

	//����һ��epoll��������
	int epfd = epoll_create(MAXSIZE);
	if (epfd == -1) {
		perror("epoll_create error");
		exit(1);
	}

	//����lfd,�������������
	int lfd = init_listen_fd(port, epfd);

	while (1)
	{
		//�����ڵ��Ӧ�¼�   -1����������
		int ret = epoll_wait(epfd, all_events, MAXSIZE, -1);
		if (ret == -1) {
			//����Ҫ��������
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
			//ֻ������¼��� �����¼�Ĭ�ϲ�����
			struct epoll_event* pev = &all_events[i];

			//���Ƕ��¼�
			if (!(pev->events & EPOLLIN)) {
				continue;
			}
			if (pev->data.fd == lfd) {  //������������
				do_accept(lfd, epfd);
			}
			else { //������
				do_read(pev->data.fd, epfd);
			}
		}

	}

}


int main(int argc, char* argv[]) {

	//�����в�����ȡ   �˿� �� server�ṩ��Ŀ¼
	if (argc < 3)
	{
		cout << "./server port path" << endl;
	}
	cout << "hello world" << endl;
	//��ȡ�û�����Ķ˿�
	int port = atoi(argv[1]);

	//�ı���̹���Ŀ¼
	int ret = chdir(argv[2]);
	if (ret != 0) {
		perror("chdir error");
		exit(1);
	}

	//���� epoll����
	epoll_run(port);

	return 0;
}



















