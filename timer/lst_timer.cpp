#include "lst_timer.h"
#include "../http/http_conn.h"

void time_wheel::add_timer(util_timer *timer)
{
    if (!timer) return ;

    int ticks = 0;
    if (timer->expire < SI) ticks = 1;
    else ticks = timer->expire / SI;

    int rotation = ticks / N;
    int ts = ( cur_slot + (ticks % N) ) % N;
    timer->rotation = rotation;
    timer->time_slot = ts;

    if ( !slot[ts] ) slot[ts] = timer;
    else 
    {
        timer->next = slot[ts];
        slot[ts] -> prev = timer;
        slot[ts] = timer;
    }
}

void time_wheel::adjust_timer(util_timer *timer)
{
    if (!timer) return ;

    int ts = timer->time_slot;
    if (timer == slot[ts]) 
    {
        slot[ts] = slot[ts]->next;
        if (slot[ts]) slot[ts]->prev = nullptr;
    } 
    else 
    {
        timer->prev->next = timer->next;
        if (timer -> next) timer->next->prev = timer->prev;
    }

    int ticks = 0;
    if (timer->expire < SI) ticks = 1;
    else ticks = timer->expire / SI;

    int rotation = ticks / N;
    ts = ( cur_slot + (ticks % N) ) % N;
    timer->rotation = rotation;
    timer->time_slot = ts;

    if ( !slot[ts] ) slot[ts] = timer;
    else 
    {
        timer->next = slot[ts];
        slot[ts] -> prev = timer;
        slot[ts] = timer;
    }
}

void time_wheel::del_timer(util_timer *timer)
{
    if (!timer) return ;

    int ts = timer->time_slot;
    if (timer == slot[ts]) 
    {
        slot[ts] = slot[ts]->next;
        if (slot[ts]) slot[ts]->prev = nullptr;
        delete timer;
    } 
    else 
    {
        timer->prev->next = timer->next;
        if (timer -> next) timer->next->prev = timer->prev;
        delete timer;
    }
}
void time_wheel::tick()
{
    util_timer* tmp = slot[cur_slot];
    while (tmp)
    {
        if (tmp->rotation > 0)
        {
            --tmp->rotation;
            tmp = tmp->next;
        }
        else
        {
            tmp->cb_func(tmp->user_data);

            if (tmp == slot[cur_slot])
            {
                slot[cur_slot] = slot[cur_slot]->next;
                if (slot[cur_slot]) slot[cur_slot]->prev = nullptr;

                delete tmp;
                tmp = slot[cur_slot];
            }
            else
            {
                tmp->prev->next = tmp->next;
                if (tmp -> next) tmp->next->prev = tmp->prev;
                util_timer* tmp2 = tmp -> next;
                delete tmp;
                tmp = tmp2;
            }
        }
    }

    ++cur_slot;
    if (cur_slot >= N) cur_slot -= N;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    // alarm(m_TIMESLOT);
    alarm(time_wheel::SI);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
