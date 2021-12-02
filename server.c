#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>

#define BACKLOG 5
void handleCommand();

int main(int argc, char **argv)
{
        if (argc != 2) {
                printf("Useage: %s <listen_port>\n\n",argv[0]);
                return -1;
        }
        int iSocketFD = 0; //socket句柄
        int iRecvLen = 0; //接收成功后的返回值
        int new_fd = 0; //建立连接后的句柄
        char buf[4096] = {0};
        struct sockaddr_in stLocalAddr = {0}; //本地地址信息
        struct sockaddr_in stRemoteAddr = {0}; //对端地址信息
        socklen_t socklen = sizeof(stRemoteAddr);

        iSocketFD = socket(AF_INET, SOCK_STREAM, 0);
        if (0 > iSocketFD)
        {
                printf("创建socket失败! \n");
                return 0;
        }


        stLocalAddr.sin_family = AF_INET;
        stLocalAddr.sin_port = htons(atoi(argv[1]));
        stLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        //绑定地址结构和socket
        if(0 > bind(iSocketFD, (void *)&stLocalAddr, sizeof(stLocalAddr)))
        {
                printf("绑定失败!\n");
                return 0;
        }

        if(0 > listen(iSocketFD, BACKLOG))
        {
                printf("监听失败\n");
                return 0;
        }

        printf("listen on port: %s\n", argv[1]);
        new_fd = accept(iSocketFD, (struct sockaddr *)&stRemoteAddr, &socklen);
        if(0 > new_fd)
        {
                printf("接收失败! error: %d \n", new_fd);
                printf(strerror(errno));
                return 0;
        }else{
                printf("连接建立成功!");
        }
        handleCommand(new_fd);
/*      printf("new_fd: %d\n", new_fd);
        iRecvLen = recv(new_fd, buf, sizeof(buf), 0);
        if(0 >= iRecvLen)
        {
                printf("接收失败或者对端关闭连接！\n");
        }else{
                printf("buf: %s\n",buf);
        }
*/
        close(new_fd);
        close(iSocketFD);
        return 0;
}

void handleCommand(int fd)
{
        while(1)
        {
                char command[10];
                char *opt;
                char *buff;
                printf(">");
                fgets(command, 10, stdin);
                command[strlen(command) - 1] = '\0';

                if (strcmp(command,"exit") == 0)
                        break;

                buff = command;
                opt = strsep(&buff," ");


                if(strcmp(opt,"send") == 0)
                {
                        opt = strsep(&buff," ");
                        if(opt == NULL)
                        {
                                printf("\terror!!!\n\tsend <count>\n");
                                break;
                        }
                        int i;
                        for(i=0;i<atoi(opt);i++)
                        {
                                send(fd, "sendtes123213213213212132132132131323233313132131231321321321321321321321321321321321321321321t1\n", sizeof("sendtes123213213213212132132132131323233313132131231321321321321321321321321321321321321321321t1\n"), 0);
                                printf("handle send: %d\n",atoi(opt));
                        }
                }
                else
                        printf("error!!!not found command\nhelp\n\tsend <count>\n\texit\n");

                opt = strsep(&buff," ");
                if(opt == NULL) continue;
                printf("opt: %s\n",opt);
        }
}

