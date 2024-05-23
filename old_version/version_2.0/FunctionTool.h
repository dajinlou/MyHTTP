#pragma once
class FunctionTool
{

public:
	//获取一行 \r\n结尾的数据
	static int get_line(int cfd, char* buf, int size);

	//以空格分割字符串  返回值为分割位置(空格位置)
	static int splitBySpace(int start, int end, char* src, char* buf);

	//通过文件名获取文件的类型
	static const char* get_file_type(const char* name);

	//编码
	static void encode_str(char* to, int tosize, const char* from);

	//解码

	static void decode_str(char* to, char* from);


private:
	//16进制 转10进制
	int hexit(char c);
};

