#ifndef __QUICKTRACE_H_
#define __QUICKTRACE_H_

#if defined _WIN32 || defined _WIN64
#define __IS_WINDOWS_ 1
//Windows
#include <winsock2.h>
#include <process.h>
//constants
#include <ws2tcpip.h.>

#pragma comment(lib, "Ws2_32.lib")

#else
// *NIX - tested on OSX and LINUX
#include <sys/time.h>
#include <sys/socket.h>

#ifndef _MSC_VER_WITH_GCC    // for compiling MSVC-specific code woth GCC
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#endif

#define __FAVOR_BSD 1

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
#define closesocket(x) close(x)
#define WSACleanup()    //WSACleanup()
#endif

using namespace std;
#include <vector>

// ICMP packet types
#define ICMP_ECHO_REPLY          0
#define ICMP_HOST_UNREACHABLE    3
#define ICMP_TIME_EXCEEDED       11
#define ICMP_ECHO_REQUEST        8

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
    QTRACE_ERR_RAW_SOCK_PERM,
    QTRACE_ERR_SOCK_SETOPT,
    QTRACE_ERR_SOCK_GETOPT,
    QTRACE_ERR_SOCK_SENDTO,
    QTRACE_ERR_SOCK_SELECT,
    QTRACE_ERR_INDEX_OUT_OF_BOUNDS,
    QTRACE_RESULT_CODE_MAX,
    QTRACE_ERR = -1
};

static const char * const qtrace_result_str[] = {
    "OK",
    "Unknown result code",
    "Error: Winsock WSAStartup error",
    "Error: Hostname unresolved",
    "Error: Socket open failed",
    "Error: Raw sockets need root privileges",
    "Error: Socket setsockopt",
    "Error: Socket getsockopt",
    "Error: Socket sendto",
    "Error: Socket select",
    "Error: Index out of bounds"
};


#ifdef _MSC_VER
#pragma pack(push, 1)
#define __PACK
#else
#define __PACK __attribute__((__packed__))
#endif

#if defined(__IS_WINDOWS_) || defined(_MSC_VER_WITH_GCC)
// The IP header
struct ip {
 unsigned char          ip_hl:4, ip_v:4; // Length of header in dwords, Version of IP
 unsigned char          ip_tos;         // Type of service
 unsigned short int     ip_len;         // Length of the packet in dwords
 unsigned short int     ip_id;          // unique identifier
 unsigned short int     ip_off;         // Flags
 unsigned char          ip_ttl;         // Time to live
 unsigned char          ip_p;           // Protocol number (TCP, UDP etc)
 unsigned short int     ip_sum;         // IP checksum
 in_addr                ip_src;
 in_addr                ip_dst;
} __PACK;

// UDP header
struct udphdr {
    unsigned short int  uh_sport;
    unsigned short int  uh_dport;
    unsigned short int  uh_ulen;
    unsigned short int  uh_sum;
} __PACK;

// ICMP header
struct icmp {
    unsigned char       icmp_type;
    unsigned char       icmp_code;
    unsigned short      icmp_cksum;
    //The following data structures are ICMP type specific
    unsigned short      icmp_id;
    unsigned short      icmp_seq;
} __PACK;

#endif  //end win ifdef

#ifdef __CYGWIN__
#ifndef IP_HDRINCL
#define IP_HDRINCL 2
#endif

// ICMP header
struct icmp {
    unsigned char       icmp_type;
    unsigned char       icmp_code;
    unsigned short      icmp_cksum;
    //The following data structures are ICMP type specific
    unsigned short      icmp_id;
    unsigned short      icmp_seq;
} __PACK;
#endif  //end CYGWIN ifdef

struct pseudoheader {
    unsigned int        src_addr;
    unsigned int        dst_addr;
    unsigned char       zero;
    unsigned char       protocol;
    unsigned short      length;
} __PACK;

//#define __USE_ICMP_ONLY__ 1

#define QTRACE_MAX_TTL_LIMIT            64  //64 hops ought to be enough for anybody
#define QTRACE_MAX_TTL_DEFAULT          32
#define QTRACE_DEFAULT_TIMEOUT_MS       1000
#if defined __IS_WINDOWS_
// Higher inter-probe interval for Windows because it uses ICMP probes, and many
// Internet routers appear to throttle or drop ICMP packets if sent too rapidly.
#define QTRACE_INTER_SEND_INTERVAL_MS   100
#else
#define QTRACE_INTER_SEND_INTERVAL_MS   10
#endif
#define QTRACE_SELECT_TIMEOUT_MS        10
#define QTRACE_DATA_SIZE                56
#define QTRACE_L4_HDR_SIZE              8   // ICMP or UDP

struct qtrace_packet {
    struct ip           ip;
    unsigned char       l4hdr[QTRACE_L4_HDR_SIZE];
    char                data[QTRACE_DATA_SIZE];
} __PACK;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

class quicktrace {
private:
    unsigned int        src_addr;
    unsigned short      src_port;
    unsigned int        dst_addr;
    unsigned short      dst_port;

    unsigned int        timeout_ms;
    unsigned int        interval_ms;
    unsigned int        last_hop;
    unsigned int        hop_count;

    int                 is_done;
    int                 stop_loop;

    int                 send_seq;
    int                 recv_non_block;
    int                 verbosity;
    int                 dump_packets;

    int                 result_code;

    vector<unsigned int> stop_hops;
    vector<unsigned int> hop_addresses;
    vector<double>       hop_latencies;

    vector< vector<unsigned int> > rep_hop_addr;
    vector< vector<double> >       rep_hop_lat;

    struct sockaddr_in  dest;
    struct sockaddr_in  src;
    struct pseudoheader pseudohdr;
    int                 dgram_size;
    int                 udp_hdr_len;
    int                 icmp_hdr_len;

    SOCKET              sock_send;
    SOCKET              sock_icmp;

    int                 pid;

#if defined __IS_WINDOWS_
    WSAData             wsaData;    //for winsock
    LARGE_INTEGER       freq;       //for timing
#endif

//State for sending and receiving
    struct qtrace_packet pkt;

    struct ip           *iph;
    struct udphdr       *udph;
    struct icmp         *icmph;

    sockaddr_in         to_addr;
    sockaddr_in         from_addr;
    socklen_t           from_addr_len;

    unsigned short      sport_base;
    unsigned short      sport;
    unsigned short      dport_base;
    unsigned short      dport;

    int                 acted;
    char                buffer[256];
    //char *buffer = (char*)malloc(256);
    struct ip           *rcv_iph;
    unsigned short      rcv_ip_hdr_len;
    struct icmp         *rcv_icmph;
    struct ip           *rcv_icmp_iph;
    struct udphdr       *rcv_icmp_udp;

    bool    icmp_echo_reply_recvd;
    bool    icmp_host_unreachable_recvd;
    bool    hops_exceeded;

    int     send(SOCKET sock, int send_ttl, int send_rep, int send_raw=0, int send_icmp=0);
    int     ping(SOCKET sock, int send_ttl, int send_rep);
    int     recv(SOCKET sock);

    int     set_ttl(SOCKET sock, int ttl);
    int     get_ttl(SOCKET sock);
    int     set_hdr_incl(SOCKET sock, int flag);
    int     hop_count_from_ttl(unsigned int ttl, unsigned int max_hop_recvd);

    int     set_target(unsigned int target_ip);

    int     set_target(const char *target);

    SOCKET  create_socket(int ttl, bool is_raw, int ipproto);

    void    cleanup();
    void     close_sockets();

//testing / alternate implementation flags
    int     enable_raw_send;
    int     use_icmp_only;
    int     byohdr;

public:
    quicktrace();
    ~quicktrace();

    static const char* last_error_msg();

//init functions
    int     init(int max_hops, int reps);

    void    set_verbosity(int v);
    int     get_verbosity();

    void    set_sequential_trace(int seq);
    void    set_timeout_ms(int tms);
    void    set_probe_interval_ms(int ims);
    void    add_stop_hop(unsigned int hop); //not fully implemented

//actions
    int     trace(const char *target, int max_hop_count, int rep_count);
    void    stop();
    int     done();
    int     wait();

//results
    int                 get_result_code();
    const char*         get_result_message();
    static const char*  get_result_message(int code);

    unsigned int get_target_address();

    int     get_hop_count();
    int     get_path(unsigned int *address, double *latency, int count);
    int     get_path(vector<unsigned int> &addresses, vector<double> &latencies);

    int             get_hop (int index, unsigned int *address, double *latency);
    unsigned int    get_hop_address(int index);
    double          get_hop_latency(int index);

    bool   pong_recvd();
    bool   host_reached();
    bool   max_hops_exceeded();

    static double get_time_msec() {
        double ms = 0;
#if defined __IS_WINDOWS_
/*
        SYSTEMTIME sysTime;
        GetSystemTime(&sysTime);
        ms = sysTime.wSecond*1000 + sysTime.wMilliseconds;
*/
        //ms = GetTickCount();

        LARGE_INTEGER freq;
        LARGE_INTEGER start;

        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        ms = (double(start.QuadPart)/double(freq.QuadPart))*1000;
#else
        struct timeval tval;
        gettimeofday(&tval, 0);
        ms = tval.tv_sec*1000.0 + ((double)tval.tv_usec)/1000.0;
#endif
        return ms;
    }
};

#endif
