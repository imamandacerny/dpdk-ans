/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015-2017 Ansyun <anssupport@163.com>. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Ansyun <anssupport@163.com> nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
* This program is used to test ans user space tcp stack
*/

#include <unistd.h>
#include <stdio.h>     
#include <sys/types.h>     
#include <sys/socket.h>     
#include <netinet/in.h>     
#include <arpa/inet.h>   
#include <stdlib.h>  
#include <string.h>  
#include <sys/epoll.h>  
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 5000  
#define MAX_EVENTS 10  


void set_nonblocking(int sockfd) 
{ 
    int opts; 

    opts = fcntl(sockfd, F_GETFL); 
    if(opts < 0) 
    { 
        printf("fcntl(F_GETFL) failed \n"); 
    } 
    
    opts = (opts | O_NONBLOCK); 
    if(fcntl(sockfd, F_SETFL, opts) < 0)
    { 
        printf("fcntl(F_SETFL) failed\n"); 
    } 
} 


int handle_event(struct epoll_event ev)
{
    int len;     
    int send_len;
    char recv_buf[BUFFER_SIZE];       
    char send_buf[BUFFER_SIZE];       

    if (ev.events&EPOLLIN)
    {

        while(1)
        {
            len= recv(ev.data.fd, recv_buf, BUFFER_SIZE, 0);  
            if(len > 0)  
            {  
                printf("receive from client(%d) , data len:%d \n", ev.data.fd, len);  

                sprintf(send_buf, "I have received your message.");
                
                send_len = send(ev.data.fd, send_buf, 1500, 0);  

                printf("send to client(%d) , data len:%d \n", ev.data.fd, send_len);  

            } 
            else if(len < 0)
            {
                if (errno == EAGAIN)   
                {
                    break;
                }
                else
                {
                    printf("remote close the socket \n");
                    close(ev.data.fd);
                    break;
                }
            }
            else
            {
                printf("remote close the socket \n");
                close(ev.data.fd);
                break;
            }

        }
    }
    else if (ev.events&EPOLLERR || ev.events&EPOLLHUP) 
    {
        printf("remote close the socket \n");
        close(ev.data.fd);
    }
    
    return 0;
}

int main(int argc, char * argv[])     
{ 
    int ret;
    int server_sockfd;   
    int client_sockfd;     
    struct sockaddr_in my_addr;      
    struct sockaddr_in remote_addr;     
    int sin_size;     
    
    memset(&my_addr,0,sizeof(my_addr)); 
    my_addr.sin_family=AF_INET; 
    my_addr.sin_addr.s_addr=INADDR_ANY;   
    my_addr.sin_port=htons(8000);    
  
    if((server_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)     
    {       
        perror("socket");     
        return 1;     
    }     

    if (bind(server_sockfd,(struct sockaddr *)&my_addr,sizeof(struct sockaddr))<0)     
    {     
        perror("bind");     
        return 1;     
    }     

    listen(server_sockfd,5);  
    
    sin_size=sizeof(struct sockaddr_in);   

    int epoll_fd;  
    epoll_fd=epoll_create(MAX_EVENTS);  
    if(epoll_fd==-1)  
    {  
        perror("epoll_create failed");  
        exit(0);  
    }  
    
    struct epoll_event ev;  
    struct epoll_event events[MAX_EVENTS];  
    ev.events=EPOLLIN;  
    ev.data.fd=server_sockfd;  

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_sockfd,&ev)==-1)  
    {  
        perror("epll_ctl:server_sockfd register failed");  
        exit(0);  
    }  
    int nfds;

    printf("tcp server is running \n");
    
    while(1)  
    {  
        nfds=epoll_wait(epoll_fd,events,MAX_EVENTS,-1);  
        if(nfds==-1)  
        {  
            perror("start epoll_wait failed");  
            exit(0);  
        }  
        int i;  
        for(i=0;i<nfds;i++)  
        {  
            if(events[i].data.fd==server_sockfd)  
            {  
                if((client_sockfd=accept(server_sockfd,(struct sockaddr *)&remote_addr,&sin_size))<0)  
                {     
                    printf("accept client_sockfd failed");     
                    exit(0);  
                }  
                ev.events=EPOLLIN;  
                ev.data.fd=client_sockfd;  
                if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_sockfd,&ev)==-1)  
                {  
                    printf("epoll_ctl:client_sockfd register failed");  
                    exit(0);  
                }  
                
                set_nonblocking(client_sockfd);
                
                printf("accept client %s \n",inet_ntoa(remote_addr.sin_addr));  
                
                send(client_sockfd, "I have received your message.", 20, 0);  

            }  
            else  
            {  
                ret = handle_event(events[i]);
            }  
        }  
    }  
    return 0;     
}    
