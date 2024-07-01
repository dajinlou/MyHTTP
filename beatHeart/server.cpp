#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>
using namespace std;

#define HEART_COUNT 5
#define BUF_SIZE 512
#define ERR_EXIT(m)         \
    do                      \
{                       \
    perror(m);          \
    exit(EXIT_FAILURE); \
} while (0)

typedef map<int, pair<string, int> > FDMAPIP;

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

//支线程传递的参数结构体
struct myparam
{
    FDMAPIP *mmap;
};

//心跳线程
void *heart_handler(void *arg)
{
    printf("the heart-beat thread started\n");
    struct myparam *param = (struct myparam *)arg;
    //Server *s = (Server *)arg;
    while (1)
    {
        FDMAPIP::iterator it = param->mmap->begin();
        for (; it != param->mmap->end();)
        {
            // 3s*5没有收到心跳包，判定客户端掉线
            if (it->second.second == HEART_COUNT)
            {
                printf("The client %s has be offline.\n", it->second.first.c_str());

                int fd = it->first;
                close(fd);               // 关闭该连接
                param->mmap->erase(it++); // 从map中移除该记录
            }
            else if (it->second.second < HEART_COUNT && it->second.second >= 0)
            {
                it->second.second += 1;
                ++it;
            }
            else
            {
                ++it;
            }
        }
        sleep(3); // 定时三秒
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

    //初始化socket元素
    struct sockaddr_in server_addr;
    int server_len = sizeof(server_addr);
    memset(&server_addr, 0, server_len);

    server_addr.sin_family = AF_INET;
    //server_addr.sin_addr.s_addr = inet_addr("0.0.0.0"); //用这个写法也可以
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(39002);

    //绑定文件描述符和服务器的ip和端口号
    int m_bindfd = bind(m_sockfd, (struct sockaddr *)&server_addr, server_len);
    if (m_bindfd < 0)
    {
        ERR_EXIT("bind ip and port fail");
    }

    //进入监听状态，等待用户发起请求
    int m_listenfd = listen(m_sockfd, 20);
    if (m_listenfd < 0)
    {
        ERR_EXIT("listen client fail");
    }

    //定义客户端的套接字，这里返回一个新的套接字，后面通信时，就用这个m_connfd进行通信
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int m_connfd = accept(m_sockfd, (struct sockaddr *)&client_addr, &client_len);

    printf("client accept success\n");

    string ip(inet_ntoa(client_addr.sin_addr)); // 获取客户端IP

    // 记录连接的客户端fd--><ip, count>，暂时就一个
    FDMAPIP mmap;
    mmap.insert(make_pair(m_connfd, make_pair(ip, 0)));

    struct myparam param;
    param.mmap = &mmap;

    // 创建心跳检测线程
    pthread_t id;
    int ret = pthread_create(&id, NULL, heart_handler, (void*)&param);
    if (ret != 0)
    {
        printf("can't create heart-beat thread.\n");
    }

    //接收客户端数据，并相应
    char buffer[BUF_SIZE];
    while (1)
    {
        if (m_connfd < 0)
        {
            m_connfd = accept(m_sockfd, (struct sockaddr *)&client_addr, &client_len);
            printf("client accept success again！！！\n");
        }

        PACKET_HEAD head;
        int recv_len = recv(m_connfd, &head, sizeof(head), 0); // 先接受包头
        if (recv_len <= 0)
        {
            close(m_connfd);
            m_connfd = -1;
            printf("client head lose connection！！！\n");
            continue;
        }

        if (head.type == HEART)
        {
            mmap[m_connfd].second = 0;
            printf("receive heart beat from client.\n");
        }
        else
        {
            //接收数据部分
        }
    }

    //关闭套接字
    close(m_connfd);
    close(m_sockfd);

    printf("server socket closed!!!\n");

    return 0;
}

