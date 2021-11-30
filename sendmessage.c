#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#define MAXLINE 1024

//tcp校验码需要的额外信息
struct udp_front
{
	uint32_t srcip; //源ip
	uint32_t desip; //目的ip
	u_int8_t zero;  
	u_int8_t protocol;  //协议类型
	u_int16_t len;   //协议长度
};

struct flags 
{
	unsigned short fin;
	unsigned short syn;
	unsigned short rst;
	unsigned short psh;
	unsigned short ack;
	unsigned short urg;
	
};

int read_data(int * ip_seq,unsigned int * tcp_seq,unsigned int * remote_seq);
int write_data(int ip_seq,unsigned int tcp_seq, unsigned int remote_seq);
//计算三层校验码
u_int16_t in_chksum(u_int16_t *addr, int len);
//计算四层校验码
u_int16_t tcp_check(char *sendbuf, int len, const struct udp_front front);
//构造syn数据包
int make_message(char *sendbuf, int send_buf_len, char **argv, unsigned int ack_seq, struct flags fg);
int my_seq;
//acked_my_tcp_seq: 被对端确认过的seq号，即下一个数据包的seq号
//remote_seq：上一次收到的数据包的seq号
unsigned int acked_my_tcp_seq,remote_seq;

int main(int argc, char **argv)
{
	if (argc != 5) {
		printf("Useage: %s <source_ip> <source_port> <dest_ip> <dest_port>\n\n",argv[0]);
		return -1;
	}
	read_data(&my_seq,&acked_my_tcp_seq,&remote_seq);
	printf("my_seq: %d,acked_my_tcp_seq:%u,remote_seq: %u\n",my_seq,acked_my_tcp_seq,remote_seq);
	int raw_sockfd,i;
	int size = 1024;
	char send_message[MAXLINE];
	struct sockaddr_in server_address;
	struct flags fg;
	
	//创建原始socket
	raw_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	//创建套接字地址
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(argv[3]);
	//设置套接字随数据包含IP首部(设置这个选项后需要我们手动写IP头)
	setsockopt(raw_sockfd, IPPROTO_IP, IP_HDRINCL, &size, sizeof(size));

	bzero(&send_message, sizeof(send_message));
	
	bzero(&fg,sizeof(fg));
	fg.psh = 1;
	fg.ack = 1;
	my_seq++;
	for(i=0;i<5;i++)
	{
		//拼接完整的TCP数据包(IP头+TCP头+数据)
		int mesg_len = make_message(send_message, MAXLINE, argv,remote_seq,fg);
		printf("mesg_len: %d\n",mesg_len);

		//发送fin消息
		sendto(raw_sockfd,send_message, mesg_len, 0, (struct sockaddr *)&server_address, sizeof(server_address));
		acked_my_tcp_seq += 12;
	}
	//接收syn+ack消息
	close(raw_sockfd);
	write_data(my_seq,acked_my_tcp_seq,remote_seq);
	return 0;
}

int print_hex(unsigned char *data, int len)
{
    int i;
    for(i=0; i<len; i++){                             //输出接收到的报文
        if(i % 16 == 0)
            printf("\n");
        printf("%02x ", data[i]);
    }
    return 0;
}

uint16_t in_chksum(uint16_t *addr, int len)
{
	int nleft = len;
	uint32_t sum = 0;
	uint16_t *w = addr;
	uint16_t answer = 0;
	while (nleft > 1)               //每次取两个字节，进行累加
	{
		sum += *w++;
		nleft -= 2;
	}
	if (nleft == 1)                //如果存在单字节，加入到累计值
	{
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}
	sum = (sum>>16) + (sum&0xffff);    // 累加的结果如果大于2字节，就把超出的部分，加入累计值
	sum += (sum>>16);                  
	answer = ~sum;			   // 按位取反，取反后可以不依赖系统大端小端
	return answer;
}

//tcp 求校验码需要额外的信息
unsigned short tcp_check(char *sendbuf, int len, const struct udp_front front)
{
	char str[MAXLINE];
	bzero(&str, MAXLINE);
	bcopy(&front, str, sizeof(front));
	bcopy(sendbuf,str+sizeof(front), len);
	return in_chksum((unsigned short *)str, sizeof(front) + len);
} 


int make_message(char *sendbuf, int send_buf_len, char **argv, unsigned int ack_seq, struct flags fg)
{
	char message[]={"testtesttest"};
	printf("Data OK: ->%s\n", message);
	struct iphdr *ip;
	ip = (struct iphdr *)sendbuf;
	ip->ihl = sizeof(struct iphdr) >> 2;
	ip->version = 4;
	ip->tos = 0;
	ip->tot_len = 0;
	ip->id = htons(my_seq);
	ip->frag_off = 0;
	ip->ttl = 128;
	ip->protocol = IPPROTO_TCP;
	ip->check = 0;
	
	ip->saddr = inet_addr(argv[1]);
	ip->daddr = inet_addr(argv[3]);
	
	printf("\nihl:%x,version: %x,tos: %x,tot_len:%x,id: %x,frag_off:%x, ttl: %x\n",ip->ihl,ip->version,ip->tos,ip->tot_len,ip->id,ip->frag_off,ip->ttl);	
	struct udp_front front;
	front.srcip = ip->saddr;
	front.desip = ip->daddr;
	front.len = htons(20 + strlen(message));
	printf("__________________________________DATA:%d_________________________",strlen(message));
	front.protocol = 6;
	front.zero = 0;
	
	struct tcphdr *tcp;
	printf("\nsizeof iphdr:%d\n",sizeof(struct iphdr));
	tcp = (struct tcphdr *)(sendbuf + sizeof(struct iphdr));
	bzero(tcp, sizeof(struct tcphdr *));
	
	tcp->source = htons(atoi(argv[2]));
	tcp->dest = htons(atoi(argv[4]));
	tcp->seq = htonl(acked_my_tcp_seq);
	printf("\nack_seq:%lx,hack_seq:%lx",ack_seq+1,htonl(ack_seq+1));
	if(ack_seq != 0){
		tcp->ack_seq = htonl(ack_seq + 1);
	}
	tcp->doff = 5;
	tcp->res1 = 0;
	tcp->fin = fg.fin;
	tcp->syn = fg.syn;
	tcp->rst = fg.rst;
	tcp->psh = fg.psh;
	tcp->ack = fg.ack;
	tcp->urg = fg.urg;
	tcp->res2 = 0;
	tcp->window = htons(65535);
	tcp->check = 0;
	tcp->urg_ptr = 0;
	strcpy((sendbuf+20+20), message);

	tcp->check = tcp_check((sendbuf+20), 20+strlen(message), front);
	
	ip->tot_len = (20 + 20 + strlen(message));
	printf("ip->tot_len: %d\n", ip->tot_len);
	ip->check = in_chksum((unsigned short *)sendbuf, 20);
	return (ip->tot_len);
}
int read_data(int * ip_seq,unsigned int * tcp_seq,unsigned int * remote_seq)
{
        FILE *fp;
        if((fp=fopen("/tmp/.tcpinfo", "rb+"))==NULL){
                printf("Cannot open file strike any key exit!");
                return 1;
        }
        fscanf(fp,"%d,%u,%u\n",ip_seq,tcp_seq,remote_seq);
        fclose(fp);
}
int write_data(int ip_seq,unsigned int tcp_seq, unsigned int remote_seq)
{
         FILE *fp;
        if((fp=fopen("/tmp/.tcpinfo", "wb+"))==NULL){
                printf("Cannot open file strike any key exit!");
                return 1;
        }
        fprintf(fp, "%d,%u,%u\n",ip_seq,tcp_seq,remote_seq);
        fclose(fp);
        return 0;
}

