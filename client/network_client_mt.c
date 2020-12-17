#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/net.h>
#include <net/sock.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <asm/uaccess.h>
#include <linux/socket.h>
#include <linux/slab.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>

#define PORT 9999

#define THREAD_NUM 4
#define STR_LEN 49
struct thread_data {
    int tid;
};

struct task_struct **ts_arr;
struct thread_data **t_args;

struct socket *conn_socket[THREAD_NUM];

unsigned int inet_addr(const char *ip)
{
    int a, b, c, d;
    char arr[4];
    sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d);
    arr[0] = a; arr[1] = b; arr[2] = c; arr[3] = d;
    return *(unsigned int *)arr;
}

int tcp_client_send(struct socket *sock, const char *buf, 
                    const size_t length, unsigned long flags)
{
    struct msghdr msg;
    struct kvec vec;
    int len, written = 0, left = length;
    mm_segment_t oldmm;

    msg.msg_name    = 0;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags   = flags;
    oldmm = get_fs(); set_fs(KERNEL_DS);

repeat_send:
    vec.iov_len = left;
    vec.iov_base = (char *)buf + written;
    len = kernel_sendmsg(sock, &msg, &vec, left, left);
    
    if ((len == -ERESTARTSYS) || 
            (!(flags & MSG_DONTWAIT) && (len == -EAGAIN)))
        goto repeat_send;
    if (len > 0) {
        written += len;
        left -= len;
        if (left)
            goto repeat_send;
    }

    set_fs(oldmm);
    return written ? written:len;
}

int tcp_client_receive(struct socket *sock, char *str,
        unsigned long flags)
{
    struct msghdr msg;
    struct kvec vec;
    int len;
    int max_size = 50;

    msg.msg_name    = 0;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags   = flags;
    
    vec.iov_len = max_size;
    vec.iov_base = str;

read_again:
    len = kernel_recvmsg(sock, &msg, &vec, max_size, max_size, flags);

    if (len == -EAGAIN || len == -ERESTARTSYS) {
        pr_info("[ FAILED ], %s, error while reading: %d tcp_client_receive...\n", __func__, len);
        goto read_again;
    }

    pr_info("[ OK ], %s, the server says: %s\n", __func__, str);
    
    return len;
}

int tcp_client_connect(int tid)
{
    struct sockaddr_in saddr;
    //unsigned char destip[5] = {192,168,0,114,'\0'};
    unsigned char *dest_ip = "192.168.0.113";

    int ret = 0;

    ret = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &conn_socket[tid]);

    if (ret < 0) {
        pr_info("[ FAILED ], %s, Error: %d, while creating first socket...\n", __func__, ret);
        goto err;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = inet_addr(dest_ip);

    ret = conn_socket[tid]->ops->connect(conn_socket[tid], 
            (struct sockaddr *)&saddr, sizeof(saddr), O_RDWR);

    if (ret && (ret != -EINPROGRESS)) {
        pr_info("[ FAILED ], %s, Error: %d, while connecting using conn socket\n", __func__, ret);
        goto err;
    }
    
    return ret;
err:
    return -1;

}

int kthread_func(void *data) 
{
    struct thread_data *t_data = (struct thread_data *)data;
    int tid = t_data->tid;
    char send_buf[STR_LEN + 1]; 
    char recv_buf[STR_LEN + 1]; 
    int ret = 0;

   
    ret = tcp_client_connect(tid);
    
    if (ret) {
        return -1;
    }
   
    DECLARE_WAIT_QUEUE_HEAD(recv_wait);
    while (1) {
        //pr_info("[ OK ], %s, TID: %d\n", __func__, tid);
        if (tid % 2 == 0)
            snprintf(send_buf, sizeof(send_buf), "Mesg1 from %d", tid);
        else
            snprintf(send_buf, sizeof(send_buf), "Mesg2 from %d", tid);

        tcp_client_send(conn_socket[tid], send_buf, strlen(send_buf), MSG_WAITALL);

        wait_event_timeout(recv_wait,
                !skb_queue_empty(&conn_socket[tid]->sk->sk_receive_queue), 5*HZ);

        if (!skb_queue_empty(&conn_socket[tid]->sk->sk_receive_queue)) {
            tcp_client_receive(conn_socket[tid], recv_buf, MSG_WAITALL);
        } 
        
        if (kthread_should_stop()) {
            if (conn_socket[tid] != NULL) {
                sock_release(conn_socket[tid]);
            }
            break;
        }
    }
}

static int __init network_client_init(void)
{
    pr_info("[ INFO ], %s, network client init...\n", __func__);
    int i;

    ts_arr = (struct task_struct **)kmalloc(sizeof(struct task_struct *) * THREAD_NUM, GFP_KERNEL);
    t_args = (struct thread_data **)kmalloc(sizeof(struct thread_data *) * THREAD_NUM, GFP_KERNEL);

    for(i = 0; i < THREAD_NUM; i++){
        t_args[i] = (struct thread_data *)kmalloc(sizeof(struct thread_data), GFP_KERNEL);
        t_args[i]->tid = i;
    }

    if (!ts_arr) {
        pr_err("ts allocation failed");
        return -ENOMEM;
    }


    for (i = 0; i < THREAD_NUM; i++) {
        ts_arr[i] = kthread_run(kthread_func, (void *)t_args[i], "kth");
    }
    
    return 0;
}

static void __exit network_client_exit(void)
{
    int i;
    
    for (i = 0; i < THREAD_NUM; i++) {
        kthread_stop(ts_arr[i]);
    }

    for (i = 0; i < THREAD_NUM; i++) {
        kfree(t_args[i]);
    }
    kfree(t_args);

    kfree(ts_arr);

    pr_info("[ OK ], %s, network client exiting...\n", __func__);
}

module_init(network_client_init)
module_exit(network_client_exit)
MODULE_LICENSE("GPL");
