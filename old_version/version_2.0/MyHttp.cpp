#include "FunctionTool.h"

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
#include <sys/stat.h>
#include <dirent.h>

#include <iostream>

using namespace std;

#define INFO "["<<__FILE__<<":"<<__LINE__<<"]"
#define MAXSIZE 2048

//���ڼ�鷵��ֵ
void checkError(int status, char* str) {
	if (status == -1) {
		perror(str);
		exit(1);
	}
}

//��ʼ������
int init_listen_fd(int port, int epfd)
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
	checkError(ret, "bind error");  //���ڼ�鷵��ֵ

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
void do_accept(int lfd, int epfd)
{
	struct sockaddr_in clt_addr;
	socklen_t clt_addr_len = sizeof(clt_addr);

	int cfd = accept(lfd, (struct sockaddr*)&clt_addr, &clt_addr_len);
	checkError(cfd, "accept error");

	//��ӡ�ͻ���IP+Port
	char client_ip[64] = { 0 };

	cout << INFO << endl;
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

// �Ͽ�����
void disconnect(int cfd, int epfd) {
	//��һ�����Ӻ������ժ��
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	if (ret != 0) {
		perror("disconnect epoll_ctl error");
		exit(1);
	}
	close(cfd);
}


/*Ӧ��Э��  
 *@params: cfd �ͻ����׽���
*/
void send_respond(int cfd,int no, char* discription,const char* type, int len)
{
	char buf[1024] = {};
	sprintf(buf, "HTTP/1.1 %d %s\r\n", no, discription);
	sprintf(buf+strlen(buf), "Content-Type:%s\r\n", type);
	sprintf(buf + strlen(buf), "Content-Length:%d\r\n", len);
	//sprintf(buf, "\r\n");
	send(cfd, buf, strlen(buf), 0);
	send(cfd, "\r\n", 2, 0);

}
//���ͷ�������Ŀ¼�������
void send_dir(int cfd, const char* dirname) {
	int i, ret;

	//ƴһ��htmlҳ��
	char buf[4096] = { 0 };
	char enstr[1024] = { 0 };
	char path[1024] = { 0 };
	cout << enstr << "   =====  " << dirname << endl;
	sprintf(buf, "<html><head><title>Ŀ¼��:%s</title></head>", dirname);
	sprintf(buf + strlen(buf), "<body><h1>��ǰĿ¼:%s</h1><table>", dirname);

	

	//Ŀ¼�����ָ��
	struct dirent** ptr;
	int num = scandir(dirname, &ptr, NULL, alphasort);

	//����
	for (i = 0; i < num; ++i) {
		char* name = ptr[i]->d_name;

		//ƴ���ļ�������·��
		sprintf(path, "%s/%s", dirname, name);
		cout << INFO << endl;
		cout << path << endl;
		struct stat st;
		stat(path, &st);
		FunctionTool::encode_str(enstr, sizeof(enstr), name);

		//������ļ�
		if (S_ISREG(st.st_mode)) {
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				enstr, name, (long)st.st_size);
		}
		else if (S_ISDIR(st.st_mode)) { //�����Ŀ¼
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
				enstr,name,(long)st.st_size);
		}
		   // ������ļ�   ��ʦд��
		//if (S_ISREG(st.st_mode)) {
		//	sprintf(buf + strlen(buf),
		//		"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
		//		enstr, name, (long)st.st_size);
		//}
		//else if (S_ISDIR(st.st_mode)) {		// �����Ŀ¼       
		//	sprintf(buf + strlen(buf),
		//		"<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
		//		enstr, name, (long)st.st_size);
		//}
		ret = send(cfd, buf, strlen(buf), 0);
		if (ret == -1) {
			if (errno == EAGAIN) {
				continue;
			}
			else if (errno == EINTR) {
				continue;
			}
			else {
				perror("send error");
				exit(1);
			}
		}
		memset(buf, 0, sizeof(buf));
		//�ַ���ƴ��

	}
	sprintf(buf + strlen(buf), "</table></body></html>");
	send(cfd, buf, strlen(buf), 0);
	cout << INFO << endl;
	cout << "dir message send OK!!!!" << endl;

#if 0
	//��Ŀ¼
	DIR* dir = opendir(dirname);
	if (dir == NULL) {
		perror("opendir error");
		exit(1);
	}

	//��Ŀ¼
	struct dirent* ptr = NULL;
	while ((ptr = readdir(dir)) != NULL) {
		char* name = ptr->d_name;
	}
	closedir(dir);
#endif
	close(cfd);
}

//���ͷ����������ļ��������
void send_file(int cfd, const char* file) {

	int n = 0;
	char buf[4096]={0};

	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		//404 ����ҳ��
		cout << "open fail" << endl;
		return;
	/*	perror("open error");
		exit(1);*/
	}
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		int ret = send(cfd, buf, sizeof(buf),0);
		if (ret == -1) {  //���������۵Ļ��� �ᱨ Resource temporarily unavailable 
			printf("errno = %d\n", errno);
			if (errno == EAGAIN) {
				cout << "----------------EAGAIN" << endl;
				continue;
			}
			else if (errno == EINTR) {
				cout << "-----------------EINTR" << endl;
				continue;
			}
			else {
				perror("send error");
				exit(1);
			}

		}//end if

	}
	close(fd);
}

void send_404Page(int cfd) {
	send_respond(cfd, 404, "fail", "text/html", -1);
	send_file(cfd, "404.html");
}

//���� http����,�ж��ļ��Ƿ����
void http_request(int cfd ,const char* file) {
	struct stat sbuf;
	cout << INFO<< file << endl;
	//��ȡ�ļ���ϸ��Ϣ
	int ret = stat(file, &sbuf);
	if (ret != 0) {  //�ļ�������
		send_404Page(cfd);
		return;
	}

	if (S_ISREG(sbuf.st_mode)) { //��һ����ͨ�ļ�
		//cout << INFO << endl;
		//cout << "is a File" << endl;
		//�ط�httpЭ��Ӧ��
		// �ж�һ�������ļ�����   
		const char* type = FunctionTool::get_file_type(file);
		//send_respond(cfd,200,"OK","Content-Type:text/plain;charset=iso-8859-1",sbuf.st_size);
		char contentType[50];
		strcat(contentType, type);
		send_respond(cfd, 200, "OK", type, sbuf.st_size);

		//�ط��ͻ���������������
		send_file(cfd,file);

	}
	else if (S_ISDIR(sbuf.st_mode)) { //Ŀ¼
		//����ͷ��Ϣ text/html; charset=utf-8   //�ӱ��뼯����������
		//const char* type = FunctionTool::get_file_type(file);
		send_respond(cfd, 200, "OK","text/html", -1);
		//����Ŀ¼��Ϣ
		send_dir(cfd, file);
	}

}

void do_read(int cfd, int epfd)
{
	//read cfd С --- ��  write ��
	//��ȡһ�� httpЭ�� [get|�ļ���|Э���]

	char line[1024] = { 0 };

	int len = FunctionTool::get_line(cfd, line, sizeof(line)); //��������
	if (len == 0) {
		printf("��������⵽�ͻ��˹ر�........\n");
		disconnect(cfd, epfd);
	}
	else {
		//���
		char method[16], path[256], protocol[16];

		sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);

		//���Լ���װ��
		//int pos = FunctionTool::splitBySpace(0, len, line, method);
		//pos = FunctionTool::splitBySpace(pos + 1, len, line, path);
		//pos = FunctionTool::splitBySpace(pos + 1, len, line, protocol);
		cout<<INFO<< "method: " << method << " path:" << path << " protocol:" << protocol<<endl;

		//��ʣ�µ����ߣ���Ҫ������ӵ��������
		while (1) {
			char buf[1024] = { 0 };
			len = FunctionTool::get_line(cfd, buf, sizeof(buf));
			if (len == 1) { //˵���ǻ��з�
				break;
			}
			else if (len == -1) {
				break;
			}
		}//end while

		if (strncasecmp(method, "GET", 3) == 0) {
			char* file = path + 1;  //�ƶ��ļ�ָ��
			if (strcmp(path, "/") == 0) {
				file = "./";
			}
			// ת�� ������ʶ����������� -> ����
			// ���� %23 %34 %5f
			FunctionTool::decode_str(path, path);
			http_request(cfd,file);
		}
	}

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



















