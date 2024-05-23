#include "FunctionTool.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

int FunctionTool::get_line(int cfd, char* buf, int size)
{
	int i = 0;

	char c = '\0';
	int n;
	while ((i < size - 1) && (c != '\n')) {
		n = recv(cfd, &c, 1, 0);
		if (n > 0) {
			if (c == '\r') {
				n = recv(cfd, &c, 1, MSG_PEEK); //拷贝 模拟读一次
				if ((n > 0) && (c == '\n')) {
					recv(cfd, &c, 1, 0);  //实际读
				}
				else {
					c = '\n';
				}
			}
			buf[i] = c;
			++i;
		}
		else {
			c = '\n';
		}

	}
	buf[i] = '\0';
	if (-1 == n) {
		i = n;
	}

	return i;

	//return 0;
}
#include <iostream>
int FunctionTool::splitBySpace(int start, int end,char* src, char* buf)
{
	int i,j = 0;
	for ( i = start; i < end; ++i) {
		if (src[i] == ' ')break;
		buf[j++] = src[i];
	}
	return i;
}

const char* FunctionTool::get_file_type(const char* name)
{
	const char* dot;

	//自右向左查找'.'字符,如不存在返回NULL
	dot = strrchr(name, '.');

	if (dot == NULL) {
		return "text/plain;charset=utf-8";
	}
	if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) {
		return "image/jpeg";
	}
	if (strcmp(dot, ".mp3")==0 || strcmp(dot, ".mpeg")==0) {
		return "audio/mpeg";
	}
	if (strcmp(dot, ".mp4")==0) {
		return "video/mp4";
	}


	return "text/plain;charset=utf-8";
}

void FunctionTool::encode_str(char* to, int tosize, const char* from)
{
	int tolen;

	for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {
		if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {
			*to = *from;
			++to;
			++tolen;
		}
		else {
			sprintf(to, "%%%02x", (int)*from & 0xff);
			to += 3;
			tolen += 3;
		}
	}
	*to = '\0';
}

void FunctionTool::decode_str(char* to, char* from)
{
	FunctionTool ft;
	for (; *from != '\0'; ++to, ++from) {
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
			*to = ft.hexit(from[1]) * 16 + ft.hexit(from[2]);
			from += 2;
		}
		else {
			*to = *from;
		}
	}
	*to = '\0';
}

int FunctionTool::hexit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}
