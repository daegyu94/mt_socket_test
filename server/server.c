#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define MAX_CLIENT 4
#define BUF_SIZE 50

void *t_function(void *data);

int client_index = 0;

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage : %s [port]\n", argv[0]);
        return 1;
    }

    int server_sock, client_sock;
    pthread_t thread_client[MAX_CLIENT];

    if((server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 )
    {
        printf("socket create error\n");
        return -1;
    }

    int on = 1;
    if(setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        printf("socket option set error\n");
        return -1;
    }

    struct sockaddr_in server_addr, client_addr;
    int client_addr_size = sizeof(client_addr);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));

    if(bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
    {
        printf("bind error\n");
        return -1;
    }

    if(listen(server_sock, 5) < 0)
    {
        printf("listen error\n");
        return -1;
    }

    int cnt = 0;
    while(1) {

        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);
        printf("accept: %d\n", ++cnt);

        if(client_sock < 0) {
            printf("accept error\n");
        }

        if(client_index == MAX_CLIENT)
        {
            printf("client accept full(max client count : %d)\n", MAX_CLIENT);
            close(client_sock);
            continue;
        }

        if(pthread_create(&thread_client[client_index], NULL, t_function, (void *)&client_sock) != 0 )
        {
            printf("Thread create error\n");
            close(client_sock);
            continue;
        }

        client_index++;

        printf("client accepted(Addr: %s, Port: %d)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    }

    return 0;

}

void *t_function(void *arg)
{
    int client_sock = *((int *)arg);
    pid_t pid = getpid();      // process id
    pthread_t tid = pthread_self();  // thread id

    printf("pid:%u, tid:%x\n", (unsigned int)pid, (unsigned int)tid);

    char send_buf[BUF_SIZE];
    char recv_buf[BUF_SIZE];
    
    while(1)
    {
        memset(send_buf, 0x00, sizeof(send_buf));
        memset(recv_buf, 0x00, sizeof(recv_buf));

        if (read(client_sock, recv_buf, sizeof(recv_buf)) <= 0) {
            printf("Client %d close\n", client_sock);
            client_index--;
            close(client_sock);
            break;
        }
 
        printf("read : Client says %s\n", recv_buf);

        snprintf(send_buf, sizeof(send_buf),"TID %ld DONE", tid);
        if(write(client_sock, send_buf, sizeof(send_buf)) <= 0) {
            printf("Client %d close\n", client_sock);
            client_index--;
            close(client_sock);
            break;
        }

        //printf("write : %s\n", buf);
    }

}
