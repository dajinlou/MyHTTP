#pragma once
class FunctionTool
{

public:
	//��ȡһ�� \r\n��β������
	static int get_line(int cfd, char* buf, int size);

	//�Կո�ָ��ַ���  ����ֵΪ�ָ�λ��(�ո�λ��)
	static int splitBySpace(int start, int end, char* src, char* buf);

	//ͨ���ļ�����ȡ�ļ�������
	static const char* get_file_type(const char* name);

	//����
	static void encode_str(char* to, int tosize, const char* from);

	//����

	static void decode_str(char* to, char* from);


private:
	//16���� ת10����
	int hexit(char c);
};

