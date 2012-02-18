// quicktrace.cpp : Defines the entry point for the console application.
//
#include "quicktrace.h"
//Common includes
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>


#include <iostream>
#include <string>
#include <string.h>

#define DEBUG 0
#define debugout if(DEBUG) cout

#define DEBUGERR 0
#define debugerr if(DEBUGERR) perror

static unsigned short checksum(int start, unsigned short *addr, int len) {
    register int nleft = len;
    register int sum = 0;
    register unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }
    if (nleft == 1) {
        *(u_char *) (&answer) = *(u_char *) w;
        sum += answer;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return (answer);
}

// Different OSes use different initial TTLs for packets
// Get estimated hop count based on guesstimate initial TTL
int quicktrace::hop_count_from_ttl(unsigned int ttl, unsigned int max_hop_recvd) {
    int hops = 0;
    if(ttl <= (32 - max_hop_recvd)) {
        hops = 32 - ttl + 1;
    }
    else if(ttl <= (64 - max_hop_recvd)) {
        hops = 64 - ttl + 1;
    }
    else if(ttl <= (128 - max_hop_recvd)) {
        hops = 128 - ttl + 1;
    }
    else {
        hops = 255 - ttl;
    }
    return hops;
}

quicktrace::quicktrace() {
    src_addr    = 0;
    src_port    = 10080;
    dst_addr    = 0;
    dst_port    = 10080;

    timeout_ms  = 1500;

    hop_count   = 0;

    use_icmp    = 0;
    send_seq    = 0;

    enable_raw_send = 0;
    recv_non_block  = 0;

    is_done     = 0;
    stop_loop   = 0;
    result_code = 0;
    
    sock_udp = sock_icmp = -1;

    hops_exceeded         = false;
    icmp_echo_reply_recvd = false;
    icmp_host_unreachable_recvd = false;

    //dgram_size = sizeof(struct qtrace_packet);
    udp_hdr_len  = 8;   //sizeof(struct udphdr);
    icmp_hdr_len = 8;  //sizeof(struct icmp);  //don't use sizeof because struct icmp is too big in netinet/ip_icmp.h
}

quicktrace::~quicktrace() {
    stop_hops.clear();
    hop_addresses.clear();
    hop_latencies.clear();
    cleanup();
}

//init functions
int quicktrace::init(int max_hops, int reps) {
    pid = getpid();    //to identify our ICMP packets
    
    //initialize vectors
    hop_addresses.resize(max_hops, 0);
    hop_latencies.resize(max_hops, 0);

    rep_hop_addr.resize(max_hops, vector<unsigned int>(reps, 0));
    rep_hop_lat.resize(max_hops , vector<double>(reps, 99999.0));

#if defined _WIN32 || defined _WIN64
    //init winsock 
    if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) {
        result_code = QTRACE_ERR_WINSOCK_STARTUP;
        return QTRACE_ERR;
    }
    //init freq for timing
    QueryPerformanceFrequency(&freq);
#endif

    //set src address
    hostent* hp;
    char name[256];
    char *ip;
    if( gethostname( name, sizeof(name)) == 0) {
        debugout<<"Host name: "<<name<<flush;
        if((hp = gethostbyname(name)) != NULL) {
            int nCount = 0;
            while(hp->h_addr_list[nCount]) {
                ip = inet_ntoa(*( struct in_addr *)hp->h_addr_list[nCount]);
                nCount++;
                debugout<<"IP #"<<nCount<<": "<<ip<<std::endl;
            }
            ip = inet_ntoa(*( struct in_addr *)hp->h_addr_list[0]);
            src_addr = inet_addr(ip);
            debugout<<"--IP #"<<0<<": "<<inet_ntoa(*( struct in_addr *) &src_addr)<<std::endl;
        }
        else {
            return QTRACE_ERR;
        }
    }
    else {
        return QTRACE_ERR;
    }
    return QTRACE_OK;
}

int quicktrace::ping(SOCKET sock, int send_ttl, int send_rep) {
    return send(sock, send_ttl, send_rep, 1, 1);
}

int quicktrace::send(SOCKET sock, int send_ttl, int send_rep, int send_raw, int send_icmp) {
    int ret;
    unsigned short int hopcode = ((unsigned int)send_ttl) * 100 + (unsigned int)send_rep;
    dport = dport_base + hopcode;
    sport = sport_base + send_rep;
    char *msg = NULL;
    int len = sizeof(struct qtrace_packet);
    if(send_raw || enable_raw_send) {
        iph->ip_ttl = send_ttl;
        iph->ip_sum = 0;
        //iph->ip_sum = checksum(0, (unsigned short *)iph, (sizeof(struct ip)));

        if(send_icmp || use_icmp) {
            //init ICMP header
            int icmp_len = icmp_hdr_len + QTRACE_DATA_SIZE;
            icmph = (struct icmp *)&pkt.l4hdr.icmp;
            icmph->icmp_type  = ICMP_ECHO;
            icmph->icmp_code  = 0;
            icmph->icmp_id    = pid;
            icmph->icmp_seq   = send_rep;  //htons(hopcode);
            icmph->icmp_cksum = 0;
            icmph->icmp_cksum = checksum(0, (unsigned short *)icmph, icmp_len);
            
            iph->ip_len = len = sizeof(struct ip) + icmp_len;
        }
        else {
            //init UDP header
            udph = (struct udphdr *)&pkt.l4hdr.udp;
            udph->uh_sport    = htons(src_port);
            udph->uh_dport    = htons(dst_port);
            udph->uh_ulen     = htons(sizeof(struct udphdr) + QTRACE_DATA_SIZE);
            udph->uh_sum      = 0;
            //recalc checksums
            //len = sizeof(struct ip) + sizeof(struct udphdr) + 8;
            //iph->ip_sum         = 0;    //kernel fills in?
            udph->uh_dport    = htons(dport);
            udph->uh_sport    = htons(sport);
            udph->uh_sum      = checksum(0, (unsigned short *)&pseudohdr, (sizeof(pseudoheader)));
            iph->ip_len = len = sizeof(struct ip) + sizeof(struct udphdr) + QTRACE_DATA_SIZE;
        }
        msg = (char*)&pkt;
    }
    else {
        len = sizeof(pkt.data);
        msg = pkt.data;
        int temp_ttl = (unsigned char)send_ttl;
        if (setsockopt(sock, IPPROTO_IP, IP_TTL, (const char*)&temp_ttl, sizeof(temp_ttl)) < 0) {
            debugerr("setsockopt/ttl");
            close_sockets();
            result_code = QTRACE_ERR_SOCK_SETOPT;
            return QTRACE_ERR;
        }
    }

    to_addr.sin_port = htons(dport);
    ret = sendto( sock, 
                  msg, 
                  len, 
                  0,
                  (struct sockaddr *)&to_addr, 
                  sizeof(struct sockaddr_in));
    if(ret < 0) {
       debugerr("sendto");
    }
    rep_hop_lat[send_ttl - 1][send_rep] = get_time_msec();

    debugout<<"\tSent to "<<inet_ntoa(to_addr.sin_addr)
           <<" hop="<<send_ttl<<"/hopcode="<<hopcode
           <<" rep="<<send_rep<<" len="<<len<<" ret="<<ret
           <<" (raw="<<enable_raw_send<<" icmp="<<use_icmp<<")"
           <<" t="<<rep_hop_lat[send_ttl - 1][send_rep]<<std::endl;
    return ret;
}

int quicktrace::recv(SOCKET sock) {
    int ret = 0;
    from_addr_len = sizeof(from_addr);

    ret = recvfrom( sock, 
                    buffer, 
                    sizeof(buffer), 
                    0, 
                    (struct sockaddr *)&from_addr, 
                    &from_addr_len);
    if(ret > 0) {
        if(rcv_iph->ip_p != IPPROTO_ICMP) {
            ret = -1;    //continue only if its an icmp packet
        }
        else {
            //get packet start offset
            rcv_ip_hdr_len = rcv_iph->ip_hl << 2;
            //sanity check
            if (ret < rcv_ip_hdr_len + ICMP_MIN) {
                ret = -1;
            }
        }
    }
    return ret;
}

SOCKET quicktrace::create_socket(int ttl, bool is_raw, int ipproto) {
    //open UDP and/or raw sockets
    SOCKET sd = -1;
    if(is_raw) {
        sd = socket(AF_INET, SOCK_RAW, ipproto);
    }
    else {
        sd = socket(AF_INET, SOCK_DGRAM, ipproto);
    }

    if(sd < 0) {
        debugerr("socket");
        if(errno == EPERM) {
            result_code = QTRACE_ERR_RAW_SOCK_PERM;
        }
        else {
            result_code = QTRACE_ERR_SOCK_OPEN;
        }
        return sd;
    }

#ifdef __BIND_SOCK__
    //bind - not necessary, but #def __BIND_SOCK__ if needed on certain platforms
    struct sockaddr_in local;
    local.sin_family      = AF_INET;
    local.sin_addr.s_addr = htons(INADDR_ANY);
    local.sin_port        = htons(0);
    if(bind(sd, (struct sockaddr *)&local, sizeof(local)) < 0 ) {
        debugout<<"bind("<<ttl<<") failed"<<std::endl;
        debugerr("bind");
        shutdown(sd, 2);
        result_code = QTRACE_ERR_SOCK_SETOPT;
        sd = -1;
    }
#endif

    if(ttl > 0) {
        unsigned char temp_ttl = (unsigned char)ttl;
        if (setsockopt(sd, IPPROTO_IP, IP_TTL, 
                        (const char*)&temp_ttl, sizeof(temp_ttl)) < 0) {
            debugerr("setsockopt");
            debugout<<"setsockopt("<<ttl<<") failed"<<std::endl;
            shutdown(sd, 2);
            close(sd);
            result_code = QTRACE_ERR_SOCK_SETOPT;
            sd = -1;
        }
    }
    return sd;
}

int quicktrace::set_hdr_incl(SOCKET sock, int flag) {
    int on = flag;
    if(setsockopt(sock, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on)) < 0) {
        debugerr("set_hdr_incl");
        result_code = QTRACE_ERR_SOCK_SETOPT;
        close_sockets();
        cleanup();
        return QTRACE_ERR;
    }
    return QTRACE_OK;
}
int quicktrace::trace(const char *target, int max_hop_count, int rep_count) {
    int ret     = 0;
    unsigned int m = (unsigned int)max_hop_count;
    const unsigned int max_hops = ((m > 0) 
                                    && (m < QTRACE_MAX_TTL_LIMIT) ? 
                                        m : QTRACE_MAX_TTL_LIMIT);

    unsigned int r = (unsigned int)rep_count;
    const unsigned int reps     = (r < 32 ? rep_count : 3);

    last_hop = max_hops;

    ret = init(max_hops, reps);

    if(ret != QTRACE_OK) {
        cleanup();
        return ret;
    }
    ret = set_target(target);
    if(ret != QTRACE_OK) {
        cleanup();
        return ret;
    }
    
    if(enable_raw_send) {  //this code path may be unnecessary...
        sock_udp  = create_socket(0, true, IPPROTO_ICMP);
        sock_icmp = sock_udp;
    }
    else { //...if this works everywhere -- needs testing on win
#if defined _WIN32 || defined _WIN64
#warning "Windows - not tested or debugged on this platform yet."
        sock_udp  = create_socket(0, false, IPPROTO_UDP);
        sock_icmp = create_socket(0, true, IPPROTO_ICMP);
#elif defined(__linux__)
#warning "Linux: needs root to open raw sockets."
        sock_udp  = create_socket(0, false, IPPROTO_UDP);
        sock_icmp = create_socket(0, true, IPPROTO_ICMP);
#elif defined(__APPLE__)
        //use SOCK_DGRAM to open non-privileged raw socket
        sock_udp  = create_socket(0, false, IPPROTO_UDP);
        sock_icmp = create_socket(0, false, IPPROTO_ICMP);
#else
#warning "OS untested, may be unsupported."
        sock_udp  = create_socket(0, false, IPPROTO_UDP);
        sock_icmp = create_socket(0, true, IPPROTO_ICMP);
#endif
    }

    if((sock_udp <= 0) || (sock_icmp <= 0)) {
        debugout<<"socket() failed: "<<sock_udp<<"/"<<sock_icmp
               <<"; may need root privileges to open raw sockets."<<std::endl;
        close_sockets();
        cleanup();
        return QTRACE_ERR;
    }

    if(enable_raw_send) {
        ret = set_hdr_incl(sock_udp, 1);
        if(ret != QTRACE_OK) {
            return ret;
        }
    }

    ret = set_hdr_incl(sock_icmp, 1);    if(ret != QTRACE_OK) {
        return ret;
    }

    //build arbitrary data
    for(unsigned int i=0; i < sizeof(pkt.data); i++) {
        pkt.data[i] = 'a' + i;
    }
    src_port = 20000 + (rand() % 2000);

    //init packet headers
    iph = (struct ip *) &pkt.ip;
    iph->ip_v           = IPVERSION;
    iph->ip_hl          = sizeof(struct ip) >> 2; //5;
    iph->ip_tos         = 0;

    iph->ip_id          = 0;
    iph->ip_off         = 0;
    iph->ip_ttl         = 0;
    iph->ip_p           = IPPROTO_ICMP; //IPPROTO_UDP;
    iph->ip_len         = 0;            //Fill later or let kernel do it
    iph->ip_sum         = 0;
    iph->ip_src.s_addr  = 0;     //src_addr;
    iph->ip_dst.s_addr  = dst_addr;

    pseudohdr.src_addr  = src_addr;
    pseudohdr.dst_addr  = dst_addr;
    pseudohdr.zero      = 0;
    pseudohdr.protocol  = IPPROTO_UDP;
    pseudohdr.length    = htons(sizeof(struct udphdr) + QTRACE_DATA_SIZE);

    //init send and recv address structures
    to_addr.sin_family  = PF_INET;
    to_addr.sin_addr    = dest.sin_addr;
    to_addr.sin_port    = htons(dst_port);

    from_addr.sin_family = PF_INET;
    from_addr.sin_addr  = dest.sin_addr;
    from_addr.sin_port  = htons(src_port);
    
    //init received IP header
    rcv_iph = (struct ip*)buffer;
    rcv_ip_hdr_len      = 0;
    rcv_icmph           = NULL;
    rcv_icmp_iph        = NULL;
    rcv_icmp_udp        = NULL;

    sport_base          = src_port;
    sport               = sport_base;
    dport_base          = 33434;
    dport               = dport_base;

    unsigned int hop    = 0;
    unsigned int rep    = 0;
    unsigned int rcvrep = 0;
    unsigned int ttl    = 1;
    double now          = 0;
    unsigned int max_hops_new   = max_hops;
    unsigned int max_hop_recvd  = 0;

    //start sending and receiving in a while loop

    int acted = 0;     // total number of send & recv ops per loop
    struct timeval timeout;
    fd_set  sendset;
    fd_set  recvset;
    fd_set  errset;
    int max_sock = (int(sock_udp) > int(sock_icmp) ? 
                    int(sock_udp) : int(sock_icmp)) + 1;
    unsigned int pings = 0;

    double prev = get_time_msec();  //use this to space the outgoing packets out a bit
    now = prev + QTRACE_INTER_SEND_INTERVAL_MS;    //to get packets going quick
    unsigned int timeout_curr = QTRACE_INTER_SEND_INTERVAL_MS;
    while(!stop_loop) {
        //use select to multiplex sending and receiving
        FD_ZERO(&recvset);
        FD_ZERO(&errset);
        FD_ZERO(&sendset);
        
        acted = 0;
        
        if(ttl > max_hops_new) {
            if(++rep < reps) {
                debugout<<"Rep "<<rep<< " done! next..."<<std::endl;
                ttl = 1;
            }
        }

        //send packet every 10 msec
        if((now - prev) >= QTRACE_INTER_SEND_INTERVAL_MS) { 
            prev = now;
            if(ttl <= max_hops_new) {
                if(send_seq) {
                    acted+=2;
                    ret = send(sock_udp, ttl, rep);
                    if(ret <= 0) {
                        debugout<<"error in send "<<ret<<std::endl;
                        result_code = QTRACE_ERR_SOCK_SENDTO;
                        stop_loop = 1;
                        continue;
                    }
                    else {
                        debugout<<"sent ttl="<<ttl<<std::endl;
                    }
                    ttl++;
                }
                else {
                    //if we are still sending
                    FD_SET(sock_udp, &sendset);
                    //debugout<<"set in sendset"<<std::flush;
                }
            }
            else if(pings < reps) {
                FD_SET(sock_icmp, &sendset);
                //debugout<<"set in sendset"<<std::flush;
            }
            else {
                //We're done sending all the packets, wait one last long timeout
                timeout_curr = timeout_ms;
            }
        }

        FD_SET(sock_icmp, &recvset);
        FD_SET(sock_udp , &errset);
        FD_SET(sock_icmp, &errset);

        timeout.tv_sec  = (timeout_curr/1000);
        timeout.tv_usec = (timeout_curr - timeout.tv_sec*1000)*1000;

        ret = select(max_sock, &recvset, &sendset, &errset, &timeout);
        if(ret < 0) {
            debugerr("select");
            result_code = QTRACE_ERR_SOCK_SELECT;
            debugout<<"select errno="<<errno<<std::endl;
            close_sockets();
            stop_loop = 1;
            continue;
        }
        now = get_time_msec();        
        //Check for recvd packets
        if(FD_ISSET(sock_icmp, &recvset)) {
            //read and decode packet
            acted+=1;
            ret = recv(sock_icmp);
            unsigned short int hopcode = 0;
            if(ret > 0) {
                //else recieved packet - decode
                rcv_icmph    = (struct icmp*)(buffer + rcv_ip_hdr_len);
                rcv_icmp_iph = (struct ip*)(buffer + rcv_ip_hdr_len + icmp_hdr_len);
                unsigned int hop_addr = from_addr.sin_addr.s_addr;
                debugout<<"Recvd icmp type="<<int(rcv_icmph->icmp_type)
                        <<" code="<<int(rcv_icmph->icmp_code)
                        <<" len="<<rcv_ip_hdr_len<<"/"<<ret<<" bytes";
                //<<" from "<<inet_ntoa(*( struct in_addr *) &(((struct ip*)buffer)->ip_src));
                
                //reset hop
                hop = max_hops;

                if((rcv_icmph->icmp_type == ICMP_TIME_EXCEEDED) 
                    || (rcv_icmph->icmp_type == ICMP_HOST_UNREACHABLE)) {
#if __USE_ICMP_ONLY__    //untested option and code
                    hopcode = ntohs(rcv_icmph->icmp_seq) / 100;
                    hop = hopcode;
                    rcvrep = ntohs(rcv_icmph->icmp_seq) - hop;
#else                
                    rcv_icmp_udp = 
                        (struct udphdr*)(buffer + rcv_ip_hdr_len  
                                + icmp_hdr_len + sizeof(struct ip));
                    hopcode = ntohs(rcv_icmp_udp->uh_dport) - dport_base;
                    hop = (unsigned int)(hopcode / 100);
                    rcvrep = hopcode - (hop * 100);//ntohs(rcv_icmp_udp->uh_sport) - sport_base;
                    hop--;  // to index from 0
                    debugout<<" udp="<<(rcv_icmp_udp->uh_dport)
                            <<"="<<ntohs(rcv_icmp_udp->uh_dport)
                            <<" - " <<dport_base<<std::flush;
#endif

                    if((hop >= max_hops) || (rcvrep >= reps)) {
                        //error, possibly corrupt data, drop it
                        debugout<<" oob: hop="<<hop<<" >= max("<<max_hops
                                <<") or rep="<<rcvrep<<" >= maxrep("<<reps<<")"<<std::endl;
                    }
                    else {
                        //hop = rcv_icmp_iph->ip_ttl - 1;
                        rep_hop_addr[hop][rcvrep] = hop_addr;    //rcv_iph->ip_src;
                        hop_addresses[hop]        = hop_addr;    
                        rep_hop_lat[hop][rcvrep]  = now - rep_hop_lat[hop][rcvrep];

                        if(rcv_icmph->icmp_type  == ICMP_HOST_UNREACHABLE) {
                            if(hop_addr == dst_addr) {
                                icmp_host_unreachable_recvd = true;
                                debugout<<"\nDestination IP matched, hop="
                                        <<hop<<" tentative last_hop="<<last_hop
                                        <<"... "<<std::flush;
                                //this could be the last hop!!
                                if(hop < last_hop) {
                                    last_hop = hop;
                                }
                            }
                            //else should never happen, because for ICMP type 
                            //ICMP_HOST_UNREACHABLE, address should be destination
                        }
                        else //ICMP_HOST_UNREACHABLE packets are bad indicators of max_hop_recvd
                        if(hop > max_hop_recvd) {
                            max_hop_recvd = hop;
                        }
                    }
                }
                else if (rcv_icmph->icmp_type == ICMP_ECHO_REPLY) {
                    //if(rcv_iph->ip_src == dst_addr)
                    if(rcv_icmph->icmp_id != pid) {
                        debugout<<"\nReceived ICMP ECHO REPLY not intended for us."
                                <<" Got="<<int(rcv_icmph->icmp_id)
                                <<" expected="<<pid
                                <<", dropping..."<<std::endl;
                    }
                    else if(hop_addr == dst_addr) {
                        icmp_echo_reply_recvd = true;
                        unsigned int est_hop_count 
                            = hop_count_from_ttl(rcv_iph->ip_ttl, max_hop_recvd);
                        hop = est_hop_count - 1;
                        rcvrep = rcv_icmph->icmp_seq;
                        
                        if((hop >= max_hops) || (rcvrep >= reps)){
                            //something is very wrong!
                            debugout<<" oob: hop="<<hop<<" >= max("<<max_hops
                                    <<") or rep="<<rcvrep<<" >= maxrep("<<reps<<")! "
                                    <<"TTL is "<<int(rcv_iph->ip_ttl)
                                    <<", estimated hop count="<<est_hop_count
                                    <<" (max recvd="<<max_hop_recvd
                                    <<") tentative last_hop="<<last_hop
                                    <<", dropping..."<<std::endl;
                        }
                        else {
                            if(hop > max_hop_recvd) {
                                max_hop_recvd = hop;
                            }
                            if(hop < last_hop) {
                                last_hop = hop;
                            }
                            debugout<<"TTL is "<<int(rcv_iph->ip_ttl)
                                    <<", estimated hop count="<<est_hop_count
                                    <<" (max recvd="<<max_hop_recvd
                                    <<") tentative last_hop="<<last_hop
                                    <<" ids="<<int(rcv_icmph->icmp_id)
                                    <<"/"<<pid<<"..."<<std::endl;
                            //this must be the last hop!!
                            hop_addresses[hop]        = hop_addr;
                            rep_hop_addr[hop][rcvrep] = hop_addr;
    
                            //don't remember why I put this here now...
                            //maybe for the case where path is longer than max hops
                            if((last_hop + 1) < max_hops_new) {
                                max_hops_new = last_hop + 1;
                            }
    
                            //for pings, max_hops - 1 element in the vector contains latencies/timestamps
                            debugout<<"RTT ["<<hop<<"]["<<rcvrep<<"] from "
                                    <<rep_hop_lat[hop][rcvrep]
                                    <<"/"<<rep_hop_lat[max_hops - 1][rcvrep]
                                    <<" now="<<now<<"..."<<endl;
                            rep_hop_lat[hop][rcvrep] 
                                = now - rep_hop_lat[max_hops - 1][rcvrep];
                        }
                    }
                    else {
                        debugout<<"\nWeird, got ICMP echo reply from unintended node: "
                                <<inet_ntoa(from_addr.sin_addr)<<" for rep "
                                <<rcvrep<<"..."<<std::endl;
                    }
                }
                debugout<<" hop="<<hop<<" @ "<<inet_ntoa(from_addr.sin_addr)
                        <<" rep="<<rcvrep<<std::endl;
                
                //check if we received all intermediate hops
                if(rep >= reps) {
                    stop_loop = 1; //if we have, stop listening and break
                    unsigned int missing = 0;
                    for(unsigned int i = 0; i < last_hop; i++) {
                        missing = 0;
                        for(unsigned int j = 0; j < reps; j++) {
                            if(rep_hop_addr[i][j] == 0) {
                                missing++;
                            }
                        }
                        if(missing >= reps) {
                            //dont stop
                            debugout<<"\n\tAt hop "<<i<<"/"<<last_hop
                                    <<" missing="<<missing
                                    <<" of reps="<<reps<<std::endl;
                            stop_loop = 0;
                            break;
                        }
                    }
                    if(stop_loop) 
                        debugout<<"At hop "<<hop<<"/"<<last_hop
                                <<" got all! Stopping... "<<std::endl;
                }
            }
        }

        //Check for ready to send packets
        if(FD_ISSET(sock_udp, &sendset)) {
            acted+=2;
            //send one packet with given TTL
            ret = send(sock_udp, ttl, rep);

            if(ret < 0) {
                close_sockets();
                result_code = QTRACE_ERR_SOCK_SENDTO;
                cleanup();
                return QTRACE_ERR;
            }
            ttl++;
        }
        
        //Check for ready to send ping packets
        if(FD_ISSET(sock_icmp, &sendset)) {
            acted+=2;
            //send one packet
            //set TTL
            ret = ping(sock_icmp, max_hops, pings);
            if(ret < 0) {
                close_sockets();
                result_code = QTRACE_ERR_SOCK_SENDTO;
                cleanup();
                return QTRACE_ERR;
            }
            pings++;
        }

        //Check for errors
        if(FD_ISSET(sock_udp, &errset) || FD_ISSET(sock_icmp, &errset)) {
            //error!
            debugerr("errset"); 
            close_sockets();
            stop_loop = 1;
        }

        if((acted == 0) && (timeout_curr == timeout_ms)){
            //timedout!!
            debugout<<"timed out! ttl="<<ttl<<"/"<<max_hops_new
                    <<" reps="<<rep<<"/"<<reps<<" pings="<<pings
                    <<"/"<<reps<<" ..."<<std::endl;
            stop_loop = 1;
        }
    }   // End loop

    now = get_time_msec();

    if(max_hop_recvd < last_hop) {
        last_hop = max_hop_recvd;        
    }

    hop_count = last_hop + 1;
    if(hop_count > hop_latencies.size()) {
        hops_exceeded = true;
        hop_count = hop_latencies.size();
    }
    
    //choose the lower of measured RTTs
    for(unsigned int i = 0; i < hop_count; i++) {
        hop_latencies[i] = 999999;
        for(unsigned int r = 0; r < reps; r++) {
            if(rep_hop_lat[i][r] < hop_latencies[i]) {
                hop_latencies[i] = rep_hop_lat[i][r];
            }
        }
        if(hop_latencies[i] == 999999) {
            hop_latencies[i] = -1;
        }
    }


    if(hop_addresses[last_hop] != dst_addr) {
        //dst-addr! Not reached!
        debugout<<"Last hop ["<<last_hop<<"] "<<inet_ntoa(*(struct in_addr*)&hop_addresses[last_hop]);
        debugout<<" not matching destination IP "<<inet_ntoa(*(struct in_addr*)&dst_addr)<<std::endl;

        if((last_hop + 2) > hop_addresses.size()) {
            hop_addresses.resize((last_hop + 2), 0);
            hop_latencies.resize((last_hop + 2), -1);
        }
        last_hop++;
        hop_addresses[last_hop] = 0;
        hop_latencies[last_hop] = -10;  //-10 should signify an unknown number of intermediate hops
        last_hop++;
        hop_addresses[last_hop] = dst_addr; 
        hop_latencies[last_hop] = -10; //-10 should signify an unknown number of intermediate hops
        hop_count = last_hop + 1;
    }

    close_sockets();
    cleanup();
    return QTRACE_OK;
}

void quicktrace::close_sockets() {
    if(sock_udp > 0) {
        shutdown(sock_udp, 2);
        close(sock_udp);
    }
    if(sock_icmp != sock_udp) {
        shutdown(sock_icmp, 2);
        close(sock_icmp);
    }
}

void quicktrace::cleanup() {
    for(unsigned int i = 0; i < rep_hop_addr.size(); i++) {
        rep_hop_addr[i].clear();
    }
    rep_hop_addr.clear();

    for(unsigned int i = 0; i < rep_hop_lat.size(); i++) {
        rep_hop_lat[i].clear();
    }
    rep_hop_lat.clear();
}

int quicktrace::set_target(const char *target) {
    unsigned int addr = inet_addr(target);
    if (addr == INADDR_NONE) {
        //try looking it up
        hostent* hp = gethostbyname(target);
        if (hp) {
            // Found an address for that host, so save it
            memcpy(&dest.sin_addr, hp->h_addr, hp->h_length);
            dest.sin_family = hp->h_addrtype;
            memcpy(&addr, hp->h_addr, sizeof(addr));
            debugout<<"Target '"<<target<<"' not quad dotted ip address, resolved to "
                    <<inet_ntoa(*( struct in_addr *) &dest.sin_addr)<<" "<<std::endl;
        }
        else {
            // Not a recognized hostname either!
            debugout<<"Target '"<<target
                    <<"' not quad dotted ip address, unable to resolve!"
                    <<std::endl;            
            result_code = QTRACE_ERR_HOSTNAME_UNRESOLVED;
            return QTRACE_ERR;
        }
    }
    return set_target(addr);
}

//results
bool quicktrace::pong_recvd() { 
    return icmp_echo_reply_recvd; 
}

bool quicktrace::host_reached() { 
    return icmp_host_unreachable_recvd; 
}

bool quicktrace::max_hops_exceeded() { 
    return hops_exceeded; 
} 

int quicktrace::set_target(unsigned int target_ip) {
    dst_addr = target_ip;
    dest.sin_addr.s_addr = dst_addr;
    dest.sin_family = PF_INET;
    debugout<<"--IP #"<<1<<": "<<inet_ntoa(*( struct in_addr *) &dst_addr)<<std::endl;
    return QTRACE_OK;
}

unsigned int quicktrace::get_target_address() {
    return dst_addr;
}

void quicktrace::set_sequential_trace(int seq) {
    send_seq = seq;
}

void quicktrace::set_timeout_ms(int tms) {
    timeout_ms = tms;
}

void quicktrace::add_stop_hop(unsigned int hop) {
    stop_hops.push_back(hop);
}

//actions
void quicktrace::stop() {
    stop_loop = 1;
}

int quicktrace::done() {
    return is_done;
}

int quicktrace::wait() {
    return is_done;
}

//results
int quicktrace::get_result_code() {
    return result_code;
}

const char* quicktrace::get_result_message() {
    return get_result_message(result_code);
}

const char* quicktrace::get_result_message(int code) {
    if((code >=0) && (code < QTRACE_RESULT_CODE_MAX)) {
        return qtrace_result_str[code];
    }
    return qtrace_result_str[QTRACE_UNKNOWN_RESULT_CODE];
}

int quicktrace::get_hop_count() {
    return hop_count;
}

int quicktrace::get_path(unsigned int *address, double *latency, int count) {
    int len = (count < (int)hop_addresses.size()? count : (int)hop_addresses.size());
    for(int i = 0; i < len; i++) {
        address[i] = hop_addresses[i];
        latency[i] = hop_latencies[i];
    }
    return len;    
}

int quicktrace::get_path(vector<unsigned int> &addresses, vector<double> &latencies) {
    addresses.resize(hop_count, 0);
    latencies.resize(hop_count, 0);
    std::copy(hop_addresses.begin(), hop_addresses.begin() + hop_count, addresses.begin());
    std::copy(hop_latencies.begin(), hop_latencies.begin() + hop_count, latencies.begin());
    return addresses.size();
}

int quicktrace::get_hop(int index, unsigned int *address, double *latency) {
    if((index < 0) && (index >= (int)hop_count)) {
        result_code = QTRACE_ERR_INDEX_OUT_OF_BOUNDS;
        return QTRACE_ERR;
    }
    *address = hop_addresses[index];
    *latency = hop_latencies[index];
    return QTRACE_OK;
}

unsigned int quicktrace::get_hop_address(int index) {
    if((index < 0) && (index >= (int)hop_count)) {
        result_code = QTRACE_ERR_INDEX_OUT_OF_BOUNDS;
        return INADDR_NONE;
    }
    return hop_addresses[index];
}


double quicktrace::get_hop_latency(int index) {
    if((index < 0) && (index >= (int)hop_count)) {
        result_code = QTRACE_ERR_INDEX_OUT_OF_BOUNDS;
        return -1;
    }
    return hop_latencies[index];
}
