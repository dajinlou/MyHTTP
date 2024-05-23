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

//用于检查返回值
void checkError(int status, char* str) {
	if (status == -1) {
		perror(str);
		exit(1);
	}
}

//初始化监听
int init_listen_fd(int port, int epfd)
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
	checkError(ret, "bind error");  //用于检查返回值

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
void do_accept(int lfd, int epfd)
{
	struct sockaddr_in clt_addr;
	socklen_t clt_addr_len = sizeof(clt_addr);

	int cfd = accept(lfd, (struct sockaddr*)&clt_addr, &clt_addr_len);
	checkError(cfd, "accept error");

	//打印客户端IP+Port
	char client_ip[64] = { 0 };

	cout << INFO << endl;
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

// 断开连接
void disconnect(int cfd, int epfd) {
	//第一步：从红黑树上摘下
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	if (ret != 0) {
		perror("disconnect epoll_ctl error");
		exit(1);
	}
	close(cfd);
}


/*应答协议  
 *@params: cfd 客户端套接字
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
//发送服务器端目录给浏览器
void send_dir(int cfd, const char* dirname) {
	int i, ret;

	//拼一个html页面
	char buf[4096] = { 0 };
	char enstr[1024] = { 0 };
	char path[1024] = { 0 };
	cout << enstr << "   =====  " << dirname << endl;
	sprintf(buf, "<html><head><title>目录名:%s</title></head>", dirname);
	sprintf(buf + strlen(buf), "<body><h1>当前目录:%s</h1><table>", dirname);

	

	//目录项二级指针
	struct dirent** ptr;
	int num = scandir(dirname, &ptr, NULL, alphasort);

	//遍历
	for (i = 0; i < num; ++i) {
		char* name = ptr[i]->d_name;

		//拼接文件的完整路径
		sprintf(path, "%s/%s", dirname, name);
		cout << INFO << endl;
		cout << path << endl;
		struct stat st;
		stat(path, &st);
		FunctionTool::encode_str(enstr, sizeof(enstr), name);

		//如果是文件
		if (S_ISREG(st.st_mode)) {
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
				enstr, name, (long)st.st_size);
		}
		else if (S_ISDIR(st.st_mode)) { //如果是目录
			sprintf(buf + strlen(buf),
				"<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
				enstr,name,(long)st.st_size);
		}
		   // 如果是文件   老师写的
		//if (S_ISREG(st.st_mode)) {
		//	sprintf(buf + strlen(buf),
		//		"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
		//		enstr, name, (long)st.st_size);
		//}
		//else if (S_ISDIR(st.st_mode)) {		// 如果是目录       
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
		//字符串拼接

	}
	sprintf(buf + strlen(buf), "</table></body></html>");
	send(cfd, buf, strlen(buf), 0);
	cout << INFO << endl;
	cout << "dir message send OK!!!!" << endl;

#if 0
	//打开目录
	DIR* dir = opendir(dirname);
	if (dir == NULL) {
		perror("opendir error");
		exit(1);
	}

	//读目录
	struct dirent* ptr = NULL;
	while ((ptr = readdir(dir)) != NULL) {
		char* name = ptr->d_name;
	}
	closedir(dir);
#endif
	close(cfd);
}

//发送服务器本地文件给浏览器
void send_file(int cfd, const char* file) {

	int n = 0;
	char buf[4096]={0};

	int fd = open(file, O_RDONLY);
	if (fd == -1) {
		//404 错误页面
		cout << "open fail" << endl;
		return;
	/*	perror("open error");
		exit(1);*/
	}
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		int ret = send(cfd, buf, sizeof(buf),0);
		if (ret == -1) {  //不分类讨论的话， 会报 Resource temporarily unavailable 
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

//处理 http请求,判断文件是否存在
void http_request(int cfd ,const char* file) {
	struct stat sbuf;
	cout << INFO<< file << endl;
	//获取文件详细信息
	int ret = stat(file, &sbuf);
	if (ret != 0) {  //文件不存在
		send_404Page(cfd);
		return;
	}

	if (S_ISREG(sbuf.st_mode)) { //是一个普通文件
		//cout << INFO << endl;
		//cout << "is a File" << endl;
		//回发http协议应答
		// 判断一下请求文件类型   
		const char* type = FunctionTool::get_file_type(file);
		//send_respond(cfd,200,"OK","Content-Type:text/plain;charset=iso-8859-1",sbuf.st_size);
		char contentType[50];
		strcat(contentType, type);
		send_respond(cfd, 200, "OK", type, sbuf.st_size);

		//回发客户端请求数据内容
		send_file(cfd,file);

	}
	else if (S_ISDIR(sbuf.st_mode)) { //目录
		//发送头信息 text/html; charset=utf-8   //加编码集，反而乱码
		//const char* type = FunctionTool::get_file_type(file);
		send_respond(cfd, 200, "OK","text/html", -1);
		//发送目录信息
		send_dir(cfd, file);
	}

}

void do_read(int cfd, int epfd)
{
	//read cfd 小 --- 大  write 回
	//读取一行 http协议 [get|文件名|协议号]

	char line[1024] = { 0 };

	int len = FunctionTool::get_line(cfd, line, sizeof(line)); //读的首行
	if (len == 0) {
		printf("服务器检测到客户端关闭........\n");
		disconnect(cfd, epfd);
	}
	else {
		//拆分
		char method[16], path[256], protocol[16];

		sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol);

		//我自己封装的
		//int pos = FunctionTool::splitBySpace(0, len, line, method);
		//pos = FunctionTool::splitBySpace(pos + 1, len, line, path);
		//pos = FunctionTool::splitBySpace(pos + 1, len, line, protocol);
		cout<<INFO<< "method: " << method << " path:" << path << " protocol:" << protocol<<endl;

		//把剩下的拿走，主要不让他拥塞缓冲区
		while (1) {
			char buf[1024] = { 0 };
			len = FunctionTool::get_line(cfd, buf, sizeof(buf));
			if (len == 1) { //说明是换行符
				break;
			}
			else if (len == -1) {
				break;
			}
		}//end while

		if (strncasecmp(method, "GET", 3) == 0) {
			char* file = path + 1;  //移动文件指针
			if (strcmp(path, "/") == 0) {
				file = "./";
			}
			// 转码 将不能识别的中文乱码 -> 中文
			// 解码 %23 %34 %5f
			FunctionTool::decode_str(path, path);
			http_request(cfd,file);
		}
	}

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



















