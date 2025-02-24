#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

class util_timer;

struct client_data
{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    // time_t expire;
    int expire;
    int rotation;
    int time_slot;
    
    void (* cb_func)(client_data *);
    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

class time_wheel
{
public:
    time_wheel() : cur_slot(0)
    {
        for (int i = 0; i < N; ++i) slot[i] = nullptr;
    };
    ~time_wheel()
    {
        for (int i = 0; i < N; ++i)
        {
            util_timer* tmp = slot[i];
            while (tmp)
            {
                slot[i] = tmp->next;
                delete tmp;
                tmp = slot[i];                
            }
        }
    };

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

public:
    static const int N = 30;    //轮盘大小
    static const int SI = 1;    //每SI秒转动一次

private:
    // void add_timer(util_timer *timer, util_timer *lst_head);

    // util_timer *head;
    // util_timer *tail;

    util_timer* slot[N];
    int cur_slot;
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    // void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    time_wheel m_timer_lst;
    static int u_epollfd;
    // int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif
