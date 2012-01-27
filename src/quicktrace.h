#ifndef __QUICKTRACE_H_
#define __QUICKTRACE_H_

//Common includes
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef WIN32
//Windows
#include <winsock2.h>
#include <process.h>
//constants
#include <ws2tcpip.h.>
#else
// *NIX
#define __FAVOR_BSD
//LINUX
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sysexits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <netdb.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define SOCKET int
#endif

#include <iostream>
#include <vector>
#include <string.h>

using namespace std;

#define QTRACE_SIZEOF_DGRAM 256

// ICMP packet types
#define ICMP_ECHO_REPLY			0
#define ICMP_HOST_UNREACHABLE	3
#define ICMP_TIME_EXCEEDED		11
#define ICMP_ECHO_REQUEST		8

//IP Header field constants
#define IPVERSION 4
//#define IP_DF
//#define IP_HDRINCL


// Minimum ICMP packet size, in bytes
#define ICMP_MIN 8

enum qtrace_result { 
	QTRACE_OK = 0,
	QTRACE_UNKNOWN_RESULT_CODE,
	QTRACE_ERR_WINSOCK_STARTUP,
	QTRACE_ERR_HOSTNAME_UNRESOLVED, 
	QTRACE_ERR_SOCK_OPEN, 
	QTRACE_ERR_SOCK_SETOPT,
	QTRACE_ERR_SOCK_SENDTO,
	QTRACE_ERR_SOCK_SELECT,
	QTRACE_ERR_INDEX_OUT_OF_BOUNDS,
	QTRACE_MAX,
	QTRACE_ERR = -1
};

static const char *qtrace_result_str[] = {
	"OK",
	"Unknown result code",
	"Error: Winsock WSAStartup error",
	"Error: Hostname unresolved", 
	"Error: Sockets socket syscall",
	"Error: Sockets setsockopt syscall",
	"Error: Sockets sendto syscall",
	"Error: Sockets select",
	"Error: Index out of bounds."
};


#ifdef _MSC_VER
#pragma pack(1)
#define __PACK
#else
#define __PACK __attribute__((__packed__))
#endif

#ifdef WIN32
// The IP header
struct ip {
 unsigned char		ip_hl:4, ip_v:4;	// Length of header in dwords, Version of IP
 unsigned char		ip_tos;				// Type of service
 unsigned short int ip_len;				// Length of the packet in dwords
 unsigned short int ip_id;				// unique identifier
 unsigned short int ip_off;             // Flags
 unsigned char		ip_ttl;             // Time to live
 unsigned char		ip_p;				// Protocol number (TCP, UDP etc)
 unsigned short int ip_sum;				// IP checksum
 unsigned int		ip_src;
 unsigned int		ip_dst;
} __PACK;

// UDP header
struct udphdr {
	unsigned short int uh_sport;
	unsigned short int uh_dport;
	unsigned short int uh_ulen;
	unsigned short int uh_sum;
} __PACK;

// ICMP header
struct icmp {
	unsigned char	icmp_type;
	unsigned char	icmp_code;
	unsigned short	icmp_cksum;
	//The following data structures are ICMP type specific
	unsigned short	icmp_id;
	unsigned short	icmp_seq;
} __PACK;
#endif

#ifdef __CYGWIN__
#ifndef IP_HDRINCL
#define IP_HDRINCL 2
#endif

// ICMP header
struct icmp {
	unsigned char	icmp_type;
	unsigned char	icmp_code;
	unsigned short	icmp_cksum;
	//The following data structures are ICMP type specific
	unsigned short	icmp_id;
	unsigned short	icmp_seq;
} __PACK;
#endif

struct pseudoheader {
	unsigned int	source_addr;
	unsigned int	destination_addr;
	unsigned char	zero;
	unsigned char	protocol;
	unsigned short	length;
} __PACK;

//#define __USE_ICMP__ 1

#define QTRACE_DATA_SIZE 8

struct qtrace_packet {
	struct	ip			ip;
//#ifdef __USE_ICMP__
    union {
        struct	icmp		icmp;
//#else	
        struct	udphdr		udp;
    } l4hdr;
//#endif
	char				data[QTRACE_DATA_SIZE];
	//struct	qtrace_hdr	qt;
} __PACK;

#ifdef _MSC_VER
#pragma pack()
#endif

class quicktrace {
private:
	unsigned int		 src_addr;
	unsigned short		 src_port;
	unsigned int		 dst_addr;
	unsigned short		 dst_port;
	unsigned int		 max_hops;
	unsigned int		 reps;
	int					 timeout_ms;
	unsigned int		 last_hop;
	unsigned int         hop_count;

	int					 is_done;
	int					 stop_loop;
	
	int					 send_seq;
	int					 recv_non_block;

	int					 result_code;

	vector<unsigned int> stop_hops;
	vector<unsigned int> hop_addresses;
	vector<double>		 hop_latencies;

	vector< vector<unsigned int> > rep_hop_addr;
	vector< vector<double> >	   rep_hop_lat;

	struct sockaddr_in	dest;
	struct sockaddr_in	src;
	struct pseudoheader pseudohdr;
	int					dgram_size;
	int					udp_hdr_len;
	int					icmp_hdr_len;

	SOCKET				sock_udp;
	SOCKET				sock_icmp;	

#ifdef WIN32
    WSAData wsaData;		//for winsock
	LARGE_INTEGER freq;		//for timing
#endif

//State for sending and receiving
	struct qtrace_packet pkt;

	struct ip *iph;
	struct udphdr *udph;
	struct icmp *icmph;

	sockaddr_in to_addr;
	sockaddr_in from_addr;
	socklen_t from_addr_len;

	unsigned short sport_base;
	unsigned short sport;
	unsigned short dport_base;
	unsigned short dport;

	struct timeval timeout;
	fd_set	sendset;
	fd_set	recvset;
	fd_set	errset;
	int		max_sock;
	int		acted;
	char	buffer[256];
	//char *buffer = (char*)malloc(256);
	struct	ip*		rcv_iph;
	unsigned short	rcv_ip_hdr_len;
	struct icmp*	rcv_icmph;
	struct ip*		rcv_icmp_ip;
	struct udphdr*	rcv_icmp_udp;

	unsigned int		hop;
	unsigned int		rep;
	unsigned int		rcvrep;
	unsigned int		max_hop_recvd;
	unsigned int		len;
	unsigned int		max_hops_new;
	double	now;
	//start sending and receiving in a while loop
	unsigned int		ttl;
	unsigned long       chksum;

	int		send(SOCKET sock, int send_ttl, int send_rep);
	int		ping(SOCKET sock, int send_ttl, int send_rep);
	int     recv(SOCKET sock);

	double get_time_msec() {
		double ms = 0;
#ifdef WIN32
/*
		SYSTEMTIME sysTime;
		GetSystemTime(&sysTime);
		ms = sysTime.wSecond*1000 + sysTime.wMilliseconds;
*/		
		//ms = GetTickCount();
		LARGE_INTEGER start;
		QueryPerformanceCounter(&start);
		ms = (double(start.QuadPart)/double(freq.QuadPart))*1000;

#else
		struct timeval tval;
		gettimeofday(&tval, 0);
		ms = tval.tv_sec*1000 + ((double)tval.tv_usec)/1000.0;
#endif
		return ms;
	}

    int set_hdr_incl(SOCKET sock, int flag);
	int set_target(unsigned int target_ip);

	int set_target(char *target);

    SOCKET create_socket(int ttl, bool is_raw, int ipproto);

	void cleanup();
	void close_sockets();
	
//testing function
	int enable_raw_send;
	int use_icmp;

public:
	quicktrace();
	~quicktrace();

//init functions
	int init();

	void set_sequential_trace(int seq);

	void set_timeout_ms(int tms);
	
	void add_stop_hop(unsigned int hop);

//actions
	int trace(char *target, int max_hop_count, int rep_count);
    void stop();
	int done();
	int wait();

//results
	int get_result_code();
	const char* get_result_message();
	const char* get_result_message(int code);

	int get_hop_count();

	int get_all_hops(unsigned int *address, double *latency, int count);

	int get_hop(int count, unsigned int *address, double *latency);

    unsigned int get_target_address();
	unsigned int get_hop_address(int count);

	double get_hop_latency(int count);
};

#endif
