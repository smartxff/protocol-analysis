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
//计算三层校验码
u_int16_t in_chksum(u_int16_t *addr, int len);
//计算四层校验码
u_int16_t tcp_check(char *sendbuf, int len, const struct udp_front front);
//构造syn数据包
int make_message(char *sendbuf, int send_buf_len, char **argv);
//构造ack数据包
int make_message_ACK(char *sendbuf, int send_buf_len, char **argv, unsigned int ack_seq);
int my_seq = 100;

int main(int argc, char **argv)
{
	if (argc != 5) {
		printf("Useage: %s <source_ip> <source_port> <dest_ip> <dest_port>\n\n",argv[0]);
		return -1;
	}
	int raw_sockfd,i;
	int size = 1024;
	char send_message[MAXLINE];
	struct sockaddr_in server_address;
	
	//创建原始socket
	raw_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	//创建套接字地址
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(argv[3]);
	//设置套接字随数据包含IP首部(设置这个选项后需要我们手动写IP头)
	setsockopt(raw_sockfd, IPPROTO_IP, IP_HDRINCL, &size, sizeof(size));

	bzero(&send_message, sizeof(send_message));

	//拼接完整的TCP数据包(IP头+TCP头+数据)
	int mesg_len = make_message(send_message, MAXLINE, argv);
	printf("mesg_len: %d\n",mesg_len);

	//发送syn消息
	sendto(raw_sockfd,send_message, mesg_len, 0, (struct sockaddr *)&server_address, sizeof(server_address));
	//接收syn+ack消息
	printf("receive SYN_ACK start:-----------------------------------------\n");
	unsigned char rec[1024];
	unsigned char ipheadlen;
	// 三层数据通信会收到所有端口的包，此处简单过滤掉非目标端口的数据包
	while(1){
		int n = recvfrom(raw_sockfd, rec, 1024, 0, NULL, NULL);
		printf("receive %d bytes:\n", n);  
		print_hex(rec, n);      //输出接收到的报文
		printf("\n\nreceive SYN_ACK end: ---------------------------------------\n");
		ipheadlen = rec[0];                      //取出IP数据包的长度
		ipheadlen = (ipheadlen & 0x0f);          // IP首部长度字段只占该字节后四位，将前4位全部至为0
		printf("ipheadlen: %d\n", ipheadlen);
		ipheadlen = 4 * ipheadlen;                //IP报头区域的数值表示整个IP报头的长度，单位是4bytes
		//版本 4 + 头部长度 4 + 服务类型 8
		// 检查目的端口是否正常
        	unsigned short dst_port = ntohs(*(unsigned short *)(rec+ipheadlen+2));
		printf("\ntcp dst port: %d,%d\n",dst_port,atoi(argv[2]));	
		if(atoi(argv[2]) == dst_port )
			break;
	}
	unsigned short iplength = ntohs(*((unsigned short *)(rec+2))); //获取IP数据报长度
	printf("iplengthAll: %d\n", iplength);
	// ip头部 + 4(端口) + 4(seq) 
	// ntohl 从网络字节顺序转换为主机字节顺序 
	unsigned int seq = ntohl(*((signed int*)(rec+ipheadlen+4)));
	//确认号
	unsigned int ack = ntohl(*((signed int*)(rec+ipheadlen+8)));
	unsigned char flag = rec[ipheadlen + 14];
	printf("flag: %02x\n", flag);

	flag = (flag & 0x12);   // 只需要ACK和SYn标志的值，其他位都清0
	if(flag != 0x12){       // 判断是否为syn+ack包
		printf("ACK+SYN\n");
	}else{
		printf("OK ACK+SYN\n");
		//unsigned int ack_seq = ack;
		unsigned int ack_seq = seq;
		int mesg_len_a = make_message_ACK(send_message, MAXLINE, argv, ack_seq);
		sendto(raw_sockfd, send_message, mesg_len_a, 0, (struct sockaddr *)&server_address, sizeof(server_address));
		
	}
	
	//unsigned int ack_seq = ack;
	unsigned int ack_seq = seq;
	int mesg_len_a = make_message_ACK(send_message, MAXLINE, argv, ack_seq);
	sendto(raw_sockfd, send_message,mesg_len_a, 0, (struct sockaddr *)&server_address, sizeof(server_address));
	close(raw_sockfd);
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

int make_message(char *sendbuf, int send_buf_len, char **argv)
{
	//建立握手过程中无法传输数据
	char message[]={""};
	struct iphdr *ip;
	//填充ip头信息
	ip = (struct iphdr *)sendbuf;
	ip->ihl = sizeof(struct iphdr) >> 2;    //首部长度
	ip->version = 4;	//ip协议版本
	ip->tos = 0;		//服务类型
	ip->tot_len =0;		//总长度
	ip->id = htons(my_seq);  //seq id值
	ip->frag_off = 0;	
	ip->ttl = 128;
	ip->protocol = IPPROTO_TCP;
	ip->check = 0;		//内核会算出相应的校验和
	//将点分十进制的IP转换成一个长整数整形。
	ip->saddr = inet_addr(argv[1]);
	ip->daddr = inet_addr(argv[3]);
	struct udp_front front;
	front.srcip = ip->saddr;
	front.desip = ip->daddr;
	front.len = htons(20 + strlen(message));
	printf("____________________________DATA:%d_______________________", strlen(message));
	front.protocol = 6;
	front.zero = 0;
	
	struct tcphdr *tcp;
	tcp = (struct tcphdr *)(sendbuf + sizeof(struct iphdr));
	bzero(tcp, sizeof(struct tcphdr *));
	// 填充tcp头信息
	tcp->source = htons(atoi(argv[2]));    //需要把主机序转换为网络序
	tcp->dest = htons(atoi(argv[4]));
	tcp->seq = htonl(100000000);
	
	tcp->doff = 5;     //数据偏移(TCP头部字节长度/4)
	tcp->res1 = 0;		// 保留字段(4位)
	tcp->fin = 0;		//用来释放一个连接
	tcp->syn = 1;		//表示这是一个连接请求
	tcp->rst = 0;		//用来表示tcp连接释放出现严重差错
	tcp->psh = 0;		// 推送
	tcp->ack = 0;		// 表示是一个确认
	tcp->urg = 0;		// 紧急数据
	tcp->res2 = 0;		// 保留字段
	tcp->window = htons(65535);  //初始窗口值设置
	
	tcp->check = 0;
	tcp->urg_ptr = 0;
	tcp->check = 0;                     
	strcpy((sendbuf+20+20), message);  //把mesage存入Ip+tcp头部之后
	
	tcp->check = tcp_check((sendbuf+20), 20+strlen(message), front);
	
	ip->tot_len = (20 + 20 + strlen(message));  //ip头长度+TCP头长度+数据长度 = 总长度
	printf("ip->tot_len:%d\n", ip->tot_len);
	ip->check = tcp_check((sendbuf+20), 20+strlen(message), front);

	ip->tot_len = (20 + 20 + strlen(message));
	printf("ip->tot_len:%d\n",ip->tot_len);
	ip->check = in_chksum((unsigned short *)sendbuf, 20);
	
	return (ip->tot_len);
}

int make_message_ACK(char *sendbuf, int send_buf_len, char **argv, unsigned int ack_seq)
{
	char message[]={""};
	printf("Data OK: ->%s\n", message);
	struct iphdr *ip;
	ip = (struct iphdr *)sendbuf;
	ip->ihl = sizeof(struct iphdr) >> 2;
	ip->version = 4;
	ip->tos = 0;
	ip->tot_len = 0;
	ip->id = htons(my_seq + 1);
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
	tcp->seq = htonl(100000001);
	printf("\nack_seq:%lx,hack_seq:%lx",ack_seq+1,htonl(ack_seq+1));
	tcp->ack_seq = htonl(ack_seq + 1);
	tcp->doff = 5;
	tcp->res1 = 0;
	tcp->fin = 0;
	tcp->syn = 0;
	tcp->rst = 0;
	tcp->psh = 0;
	tcp->ack = 1;
	tcp->urg = 0;
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
