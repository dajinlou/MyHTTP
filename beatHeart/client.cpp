#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <pthread.h>
#include <map>
using namespace std;


#define BUF_SIZE 512
#define ERR_EXIT(m)         \
    do                      \
{                       \
    perror(m);          \
    exit(EXIT_FAILURE); \
} while (0)

enum Type
{
    HEART,
    OTHER
};

struct PACKET_HEAD
{
    Type type;
    int length;
};

//线程传递的参数结构体
struct myparam   
{   
    int fd;  
};

void *send_heart(void *arg)
{
    printf("the heartbeat sending thread started.\n");
    struct myparam *param = (struct myparam *)arg;
    int count = 0; // 测试
    while (1)
    {
        PACKET_HEAD head;
        //发送心跳包
        head.type = HEART;
        head.length = 0;
        send(param->fd, &head, sizeof(head), 0);
        // 定时3秒，这个可以根据业务需求来设定
        sleep(3); 
    }
}

int main()
{
    //创建套接字
    int m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd < 0)
    {
        ERR_EXIT("create socket fail");
    }

    //服务器的ip为本地，端口号
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(39002);

    //向服务器发送连接请求
    int m_connectfd = connect(m_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (m_connectfd < 0)
    {
        ERR_EXIT("connect server fail");
    }

    struct myparam param;
    param.fd = m_sockfd;
    pthread_t id;
    int ret = pthread_create(&id, NULL, send_heart, (void*)&param);
    if (ret != 0)
    {
        printf("create thread fail.\n");
    }

    //发送并接收数据
    char buffer[BUF_SIZE] = "asdfg";
    int len = strlen(buffer);
    while (1)
    {
        // 发送数据部分;

    }

    //断开连接
    close(m_sockfd);

    printf("client socket closed!!!\n");

    return 0;
}

