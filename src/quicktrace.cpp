// quicktrace.cpp : Defines the entry point for the console application.
//
#include "quicktrace.h"

#define DEBUG 0
#define debugout if(DEBUG) cout

static unsigned short checksum(int start, unsigned short *addr, int len) {
	register int sum = 0;
	unsigned short answer = 0;
	register unsigned short *w = addr;
	register int nleft = len;
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

quicktrace::quicktrace() {
	src_addr	= 0;
	src_port	= 10080;
	dst_addr	= 0;
	dst_port	= 10080;
	max_hops	= 1;
	reps		= 1;
	timeout_ms	= 1500;

	last_hop	= max_hops;
	hop_count	= 0;

	enable_raw_send = 0;
	use_icmp = 0;
	send_seq = 0;
	recv_non_block = 0;

	is_done		= 0;
	stop_loop	= 0;
	result_code	= 0;
	
	sock_udp = sock_icmp = -1;

	//dgram_size = sizeof(struct qtrace_packet);
	udp_hdr_len  = 8;   //sizeof(struct udphdr);
	icmp_hdr_len = 8;  //sizeof(struct icmp);
}

quicktrace::~quicktrace() {
	stop_hops.clear();
	hop_addresses.clear();
	hop_latencies.clear();
	cleanup();
}

//init functions
int quicktrace::init() {
	//initialize vectors
	hop_addresses.resize(256, 0);
	hop_latencies.resize(256, 0);

	rep_hop_addr.resize(256, vector<unsigned int>(reps, 0));
	rep_hop_lat.resize(256, vector<double>(reps, 99999.0));

#ifdef WIN32
	//init winsock 
	if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) {
        //cerr<<"Failed to find Winsock 2.1 or better."<<endl;
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
    int ret = 0;
    int prev_enable_raw_send = enable_raw_send;
    int prev_use_icmp = use_icmp;
    enable_raw_send = 1;
    use_icmp = 1;

    ret  = send(sock, send_ttl, send_rep);

    enable_raw_send = prev_enable_raw_send;
    use_icmp = prev_use_icmp;
    return ret;
}

int quicktrace::send(SOCKET sock, int send_ttl, int send_rep) {
	int ret;
	unsigned short int hopcode = send_ttl * 100 + send_rep;
	dport = dport_base + hopcode;
	sport = sport_base + send_rep;
	char *msg = NULL;
	if(enable_raw_send) {
		iph->ip_ttl = send_ttl;
		iph->ip_sum = 0;
		iph->ip_sum = checksum(0, (unsigned short *)iph, (sizeof(struct ip))) + 1;
		len = sizeof(struct qtrace_packet);

        if(use_icmp) {
            //init ICMP header
        	icmph = (struct icmp *)&pkt.l4hdr.icmp;
        	icmph->icmp_type  = ICMP_ECHO;
        	icmph->icmp_code  = 0;
        	icmph->icmp_cksum = 0;
        	//The following data structures are ICMP type specific
        	icmph->icmp_id = getpid();	//pid
        	icmph->icmp_seq = 0;

    		//recalc checksums
    		//len = sizeof(struct ip) + icmp_hdr_len;
            //iph->ip_sum = checksum(0, (unsigned short *)iph, (sizeof(struct ip))) + 1;
            //iph->ip_sum         = 0;    //kernel fills in?
    		icmph->icmp_seq 	= send_rep;  //htons(hopcode);
    		icmph->icmp_cksum 	= 0;
    		icmph->icmp_cksum 	= checksum(chksum, (unsigned short *)icmph, icmp_hdr_len + QTRACE_DATA_SIZE);
    		//pkt.data[0] = send_ttl;
    		//pkt.data[1] = send_rep;
    		len = sizeof(struct qtrace_packet);   // - QTRACE_DATA_SIZE;
        }
        else {
            //init UDP header
        	udph = (struct udphdr *)&pkt.l4hdr.udp;
        	udph->uh_sport		= htons(src_port);
        	udph->uh_dport		= htons(dst_port);
        	udph->uh_ulen		= htons(sizeof(struct udphdr) + QTRACE_DATA_SIZE);
        	//chksum = checksum_start(0, (unsigned short *)&pseudohdr, (sizeof(pseudoheader)));
        	udph->uh_sum = 0;
    		//recalc checksums
    		//len = sizeof(struct ip) + sizeof(struct udphdr) + 8;
            //iph->ip_sum         = 0;    //kernel fills in?
    		udph->uh_dport	= htons(dport);
    		udph->uh_sport	= htons(sport);
    		//udph->uh_sum	= checksum(chksum, (unsigned short *)udph, (sizeof(struct udphdr)));
    		udph->uh_sum = checksum(chksum, (unsigned short *)&pseudohdr, (sizeof(pseudoheader)));
        }
        msg = (char*)&pkt;
	}
	else {
		len = sizeof(pkt.data);
		msg = pkt.data;
		int temp_ttl = (unsigned char)send_ttl;
	    //debugout<<"b1:"<<send_ttl<<":"<<int(temp_ttl)<<":"<<len<<std::endl;
		if (setsockopt(sock, IPPROTO_IP, IP_TTL, (const char*)&temp_ttl, sizeof(temp_ttl)) == -1) {
            perror("setsockopt/ttl");
            close_sockets();
            result_code = QTRACE_ERR_SOCK_SETOPT;
            return QTRACE_ERR;
		}
		//pkt.data[0] = send_ttl;
		//pkt.data[1] = send_rep;
	}
	to_addr.sin_port = htons(dport);
	ret = sendto(sock, 
				msg, 
				len, 
				0,
				(struct sockaddr *)&to_addr, 
				sizeof(struct sockaddr_in));
	if(ret < 0) {
	   perror("sendto");
	}

	rep_hop_lat[send_ttl - 1][send_rep] = get_time_msec();
	debugout<<"\n\t sent to "<<inet_ntoa(to_addr.sin_addr)<<" hop="<<send_ttl<<"/hopcode="<<hopcode<<" rep="<<send_rep<<" len="<<len<<" ret="<<ret<<std::endl;
	return ret;
}

int quicktrace::recv(SOCKET sock) {
	int ret = 0;
	from_addr_len = sizeof(from_addr);
	//memset(buffer, 0, sizeof(buffer));
	ret = recvfrom( sock, 
					buffer, 
					sizeof(buffer), 
					0, 
					(struct sockaddr *)&from_addr, 
					&from_addr_len);
	if(ret > 0) {
		if(rcv_iph->ip_p != IPPROTO_ICMP) {
			//continue only if its an icmp packet
			ret = -1;
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
	//open raw socket
	SOCKET sd = -1;
#ifdef WIN32
	#warning "Windows"
	if(is_raw) {
    	sd = socket(PF_INET, SOCK_RAW, ipproto);
	}
	else {
	   sd = socket(AF_INET, SOCK_DGRAM, ipproto);
	}
#elif defined(__linux__)
	#warning "Linux"
	if(is_raw) {
    	sd = socket(PF_INET, SOCK_RAW, ipproto);
	}
	else {
	   sd = socket(AF_INET, SOCK_DGRAM, ipproto);
	}
#elif defined(__APPLE__)
	#warning "Mac OS X"
	//sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    //sd = socket(AF_INET, SOCK_RAW, ipproto);
    sd = socket(AF_INET, SOCK_DGRAM, ipproto);  //use SOCK_DGRAM to open non-privileged raw socket
#else
	#warning "OS unsure..."
	if(is_raw) {
    	sd = socket(PF_INET, SOCK_RAW, ipproto);
	}
	else {
	   sd = socket(AF_INET, SOCK_DGRAM, ipproto);
	}#endif

    //bind 
    /*if
    struct sockaddr_in local;
    local.sin_family      = AF_INET;
    local.sin_addr.s_addr = htons(INADDR_ANY);
    local.sin_port        = htons(0);
    if(bind(sd, (struct sockaddr *)&local, sizeof(local)) < 0 ) {
        debugout<<"bind("<<ttl<<") failed"<<std::endl;
        perror("bind");
		shutdown(sd, 2);
		result_code = QTRACE_ERR_SOCK_SETOPT;
		sd = -1;
    }
    else */ 
    if(ttl > 0) {
		unsigned char temp_ttl = (unsigned char)ttl;
		if (setsockopt(sd, IPPROTO_IP, IP_TTL, (const char*)&temp_ttl, sizeof(temp_ttl)) == -1) {
	        debugout<<"setsockopt("<<ttl<<") failed"<<std::endl;
	        perror("setsockopt");
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
	if(setsockopt(sock, IPPROTO_IP, IP_HDRINCL, (char *)&on, sizeof(on)) == -1) {
	    perror("set_hdr_incl");
		result_code = QTRACE_ERR_SOCK_SETOPT;
		close_sockets();
		cleanup();
		return QTRACE_ERR;
	}
    return QTRACE_OK;
}
int quicktrace::trace(char *target, int max_hop_count, int rep_count) {
	int ret = 0;
	reps = rep_count;

	ret = init();
	if(ret != QTRACE_OK) {
		cleanup();
		return ret;
	}
	ret = set_target(target);
	if(ret != QTRACE_OK) {
		cleanup();
		return ret;
	}
	
	if(enable_raw_send) {  //this code path may be unnecessary
        sock_udp = create_socket(0, true, IPPROTO_ICMP);
        sock_icmp = sock_udp;
	}
	else { //if this works everywhere -- needs testing on win and linux
        sock_udp = create_socket(0, false, IPPROTO_UDP);
        sock_icmp = create_socket(0, false, IPPROTO_ICMP);
	}

	if((sock_udp <= 0) || (sock_icmp <= 0)) {
	    debugout<<"socket() failed"<<std::endl;
	    close_sockets();
		result_code = QTRACE_ERR_SOCK_OPEN;
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
        
	max_hops = max_hop_count;
	src_port = 20000 + (rand() % 2000);

	max_sock = (int(sock_udp) > int(sock_icmp) ? int(sock_udp) : int(sock_icmp)) + 1;
	int acted = 0;
	rcv_iph = (struct ip*)buffer;
	rcv_ip_hdr_len	 = 0;
	rcv_icmph 	 = NULL;
	rcv_icmp_ip	 = NULL;
	rcv_icmp_udp = NULL;

	sport_base = src_port;
	sport = sport_base;
	dport_base = 33434;
	dport = dport_base;

	hop = 0;
	rep = 0;
	rcvrep = 0;
	max_hop_recvd = 0;
	len = 0;
	max_hops_new = max_hops;
	now = 0;
	//start sending and receiving in a while loop
	ttl = 1;
	chksum = 0;

	//build arbitrary data
	for(unsigned int i=0; i < sizeof(pkt.data); i++) {
		pkt.data[i] = 'a' + i;
	}

	//init packet headers
	iph = (struct ip *) &pkt.ip;
	iph->ip_v	= IPVERSION;
	iph->ip_hl	= sizeof(struct ip) >> 2; //5;
	iph->ip_tos = 0;
#ifdef __APPLE__
    iph->ip_len = sizeof(struct qtrace_packet);  //ip_len is host order on mac osx
#else
    iph->ip_len = htons(sizeof(struct qtrace_packet));  //unsure about others
#endif
	iph->ip_id  = 0;
	iph->ip_off = 0;
	iph->ip_ttl = 0;
#ifdef __APPLE__
	iph->ip_p	= IPPROTO_ICMP;  //IPPROTO_UDP;
#elif defined __ICMP__
	iph->ip_p	= IPPROTO_ICMP;
#else
	iph->ip_p	= IPPROTO_UDP;
#endif
	iph->ip_sum = 0;
	memcpy(&iph->ip_src, &src_addr, sizeof(iph->ip_src));
	memcpy(&iph->ip_dst, &dst_addr, sizeof(iph->ip_dst));

	pseudohdr.source_addr = src_addr;
	pseudohdr.destination_addr = dst_addr;
	pseudohdr.zero		= 0;
	pseudohdr.protocol	= IPPROTO_UDP;
	pseudohdr.length	= htons(sizeof(struct udphdr) + QTRACE_DATA_SIZE);

	//init send and recv address structures
	to_addr.sin_family = PF_INET;
	to_addr.sin_addr = dest.sin_addr;
	to_addr.sin_port = htons(dst_port);

	from_addr.sin_family = PF_INET;
	from_addr.sin_addr = dest.sin_addr;
	from_addr.sin_port = htons(src_port);
    
    unsigned int pings = 0;
	while(!stop_loop) {
        //use select to multiplex sending and receiving
		FD_ZERO(&recvset);
		FD_ZERO(&errset);
		FD_ZERO(&sendset);
		
		acted = 0;

		if(ttl > max_hops_new) {
			if(++rep < reps) {
				debugout<<"\nRep "<<rep<< " done! next..."<<flush;
				ttl = 1;
			}
		}
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
			pings++;
			//debugout<<"set in sendset"<<std::flush;
		}

		FD_SET(sock_icmp, &recvset);
		FD_SET(sock_udp , &errset);
        FD_SET(sock_icmp, &errset);

		timeout.tv_sec	= (timeout_ms/1000);
		timeout.tv_usec = (timeout_ms - timeout.tv_sec*1000)*1000;

		ret = select(max_sock, &recvset, &sendset, &errset, &timeout);
		if(ret < 0) {
			result_code = QTRACE_ERR_SOCK_SELECT;
			debugout<<"\nselect errno="<<errno<<"\n";
			perror("select");
			close_sockets();
			stop_loop = 1;
			continue;
		}
		
		//Check for recvd packets
		if(FD_ISSET(sock_icmp, &recvset)) {
			//read and decode packet
			acted+=1;
			now = get_time_msec();
			ret = recv(sock_icmp);
			unsigned short int hopcode = 0;
			if(ret > 0) {
				//else recieved packet - decode
				rcv_icmph	 = (struct icmp*)(buffer + rcv_ip_hdr_len);
				rcv_icmp_ip  = (struct ip*)(buffer + rcv_ip_hdr_len + icmp_hdr_len);
				debugout<<"\nRecvd icmp type="<<int(rcv_icmph->icmp_type)<<" code="<<int(rcv_icmph->icmp_code)<<" len="<<rcv_ip_hdr_len<<"/"<<ret<<" bytes -> ";
				//debugout<<" from "<<inet_ntoa(*( struct in_addr *) &(((struct ip*)buffer)->ip_src));
#ifdef __USE_ICMP__
            	hopcode = ntohs(rcv_icmph->icmp_seq) / 100;
                hop = hopcode;
                rcvrep = ntohs(rcv_icmph->icmp_seq) - hop;
				//debugout<<(rcv_icmph->icmp_seq)<<"="<<ntohs(rcv_icmph->icmp_seq);
				//unsigned char *hx = (unsigned char*)(buffer + rcv_ip_hdr_len);
				//printf("\n %s [%02x %02x %02x %02x %02x %02x %02x %02x]\n", inet_ntoa(*( struct in_addr *) &(((struct ip*)buffer)->ip_src)), hx[0], hx[1], hx[2], hx[3], hx[4], hx[5], hx[6], hx[7]);
				//printf("\n %s [%02x %02x %02x %02x || %02x %02x %02x %02x]\n", inet_ntoa(*( struct in_addr *) &rcv_icmp_ip->ip_src), hx[0], hx[1], hx[2], hx[3], hx[8], hx[9], hx[10], hx[11]);
#else				
				rcv_icmp_udp = (struct udphdr*)(buffer + rcv_ip_hdr_len  + icmp_hdr_len + sizeof(struct ip));
				hopcode = ntohs(rcv_icmp_udp->uh_dport) - dport_base;
				hop = (int)(hopcode / 100);
				rcvrep = hopcode - (hop * 100);//ntohs(rcv_icmp_udp->uh_sport) - sport_base;
				hop--;  // to index from 0
				debugout<<" udp="<<(rcv_icmp_udp->uh_dport)<<"="<<ntohs(rcv_icmp_udp->uh_dport)<<" - " <<dport_base<<std::flush;
				//unsigned char *hx = (unsigned char*)(rcv_icmp_ip) + 8;
                //printf(" %s ", inet_ntoa(*( struct in_addr *) &rcv_icmp_ip->ip_src));
                //printf(" [%02x %02x %02x %02x %02x %02x %02x %02x]", hx[0], hx[1], hx[2], hx[3], hx[4], hx[5], hx[6], hx[7]);
#endif
				debugout<<" hop="<<hop<<" @ "<<inet_ntoa(from_addr.sin_addr)<<" rep="<<rcvrep<<std::flush;
				//cout<<" rcvd@="<<now<<" sent@"<<rep_hop_lat[hop][rcvrep]<<flush;

				//cout<<"\n"<<hop<<"/"<<last_hop<<" received "<<ret<<" bytes from "<<inet_ntoa(from_addr.sin_addr)<<flush;
			    if((hop >= max_hops) || (rcvrep >= reps)) {
			        //error, possibly corrupt data, drop it
                    debugout<<" hop="<<hop<<" >= max("<<max_hops<<") or rep="<<rcvrep<<" >= maxrep("<<reps<<")"<<std::flush;
                    continue;
			    }

				if(hop > max_hop_recvd) {
					max_hop_recvd = hop;
				}

				if (rcv_icmph->icmp_type == ICMP_TIME_EXCEEDED) {
					//hop = rcv_icmp_ip->ip_ttl - 1;
					rep_hop_addr[hop][rcvrep] = (*(unsigned int*)&from_addr.sin_addr);	//rcv_iph->ip_src;
					hop_addresses[hop] = (*(unsigned int*)&from_addr.sin_addr);	
					rep_hop_lat[hop][rcvrep] = now - rep_hop_lat[hop][rcvrep];
					//cout<<" lat="<<rep_hop_lat[hop][rcvrep]<<flush;
				}
				else if ((rcv_icmph->icmp_type == ICMP_HOST_UNREACHABLE) 
						|| (rcv_icmph->icmp_type == ICMP_ECHO_REPLY)) {
					//if(rcv_iph->ip_src == dst_addr) {
				    debugout<<"\nRecvd pong type="<<rcv_icmph->icmp_type<<"..."<<flush;
					if(*(unsigned int*)&from_addr.sin_addr == dst_addr) {
						//this must be the last hop!!
						hop_addresses[hop] = (*(unsigned int*)&from_addr.sin_addr);	//rcv_iph->ip_src;
						rep_hop_addr[hop][rcvrep] = (*(unsigned int*)&from_addr.sin_addr);	//rcv_iph->ip_src;
						if((last_hop+1) < max_hops_new) {
							max_hops_new = last_hop+1;
						}
						rep_hop_lat[hop][rcvrep] = now - rep_hop_lat[hop][rcvrep];

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
									stop_loop = 0;
									break;
								}
							}
						}
					}
/*
					if ((rcv_icmph->icmp_code == 1)			//HOST_UNREACHABLE
						|| (rcv_icmph->icmp_code == 2)		//PROTO_UNREACHABLE
						|| (rcv_icmph->icmp_code == 3)) {	//PORT_UNREACHABLE
						//the ICMP error incurred at the last hop
					}
*/
				}
				continue;
			}
		}

		//Check for ready to send packets
		if(FD_ISSET(sock_udp, &sendset)) {
			acted+=2;
			//send one packet
			//set TTL

			ret = send(sock_udp, ttl, rep);

			if(ret < 0) {
    			close_sockets();
				result_code = QTRACE_ERR_SOCK_SENDTO;
				cleanup();
				return QTRACE_ERR;
			}
			//cout<<"\nPacket sent "<<ret<<" bytes "<<(enable_raw_send ? "(raw)" : "")<<" with ttl="<<ttl<<flush;
			ttl++;
		}
		
		//Check for ready to send packets
		if(FD_ISSET(sock_icmp, &sendset)) {
			acted+=2;
			//send one packet
			//set TTL
		    ret = ping(sock_icmp, 255, pings);

			if(ret < 0) {
    			close_sockets();
				result_code = QTRACE_ERR_SOCK_SENDTO;
				cleanup();
				return QTRACE_ERR;
			}
			//cout<<"\nPacket sent "<<ret<<" bytes "<<(enable_raw_send ? "(raw)" : "")<<" with ttl="<<ttl<<flush;
		}

		//Check for errors
		if(FD_ISSET(sock_udp, &errset) || FD_ISSET(sock_icmp, &errset)) {
			//error! 
			close_sockets();
			stop_loop = 1;
		}

		if(acted==0) {
			//timedout!!
			stop_loop = 1;
		}
	}
	//cout<<"\nOut of loop="<<stop_loop<<endl<<flush;
	if(max_hop_recvd < last_hop) {
		last_hop = max_hop_recvd;		
	}

	hop_count = last_hop+1;
	if(hop_count > hop_latencies.size()) {
	   hop_count = hop_latencies.size();
	}
	for(unsigned int i = 0; i < hop_count; i++) {
		//cout<<"\n"<<(i+1)<<" "<<inet_ntoa(*( struct in_addr *)&hop_addresses[i])<<"\t"<<flush;
		hop_latencies[i] = 999999;
		for(rep = 0; rep < reps; rep++) {
			//cout<<rep_hop_lat[i][rep]<<" "<<flush;
			if(rep_hop_lat[i][rep] < hop_latencies[i]) {
				hop_latencies[i] = rep_hop_lat[i][rep];
			}
		}
		if(hop_latencies[i] == 999999) {
			hop_latencies[i] = -1;
		}
	}


    if(hop_addresses[last_hop] != dst_addr) {
		//dst-addr! Not reached!

        debugout<<"\nLast hop ("<<last_hop<<" hop) "<<inet_ntoa(*(struct in_addr*)&hop_addresses[last_hop]);
		debugout<<" not matching destination IP "<<inet_ntoa(*(struct in_addr*)&dst_addr)<<endl<<flush;

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
        hop_count = last_hop+1;
        /*
		//do iterative pings until we get an echo
		while(ret <=0) { 
			ttl+=4;
			double then = get_time_msec();
			ret = ping(ttl, 0);
			ret = recv();
			now = get_time_msec(); 
			if(ret > 0) {
				//estimate hop count
				if(*(unsigned int*)&from_addr.sin_addr == dst_addr) {
					last_hop = ttl;
					hop_addresses[last_hop] = dst_addr;
					hop_latencies[last_hop] = now - then;
				}
				else if(ttl >= max_hops) {
					//host unreachable
					last_hop = ttl;
					break;
				}
			}
		}
		*/
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

int quicktrace::set_target(char *target) {
	unsigned int addr = inet_addr(target);
    if (addr == INADDR_NONE) {
        //try looking it up
        hostent* hp = gethostbyname(target);
        if (hp != 0) {
            // Found an address for that host, so save it
            memcpy(&dest.sin_addr, hp->h_addr, hp->h_length);
            dest.sin_family = hp->h_addrtype;
			memcpy(&addr, hp->h_addr, sizeof(addr));
    		debugout<<"\nTarget '"<<target<<"' not quad dotted ip address, resolved to "<<inet_ntoa(*( struct in_addr *) &dest.sin_addr)<<" "<<std::endl;
        }
        else {
            // Not a recognized hostname either!
    		debugout<<"\nTarget '"<<target<<"' not quad dotted ip address, unable to resolve!"<<std::endl;            
 			result_code = QTRACE_ERR_HOSTNAME_UNRESOLVED;
            return QTRACE_ERR;
        }
    }
	return set_target(addr);
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
	if((code >=0) && (code < QTRACE_MAX)) {
		return qtrace_result_str[code];
	}
	return qtrace_result_str[QTRACE_UNKNOWN_RESULT_CODE];
}

int quicktrace::get_hop_count() {
	return hop_count;
}

int quicktrace::get_all_hops(unsigned int *address, double *latency, int count) {
	int len = (count < (int)hop_addresses.size()? count : (int)hop_addresses.size());
	for(int i = 0; i < len; i++) {
		address[i] = hop_addresses[i];
		latency[i] = hop_latencies[i];
	}
    return len;	
}

int quicktrace::get_hop(int count, unsigned int *address, double *latency) {
	if((count < 0) && (count >= (int)hop_count)) {
		result_code = QTRACE_ERR_INDEX_OUT_OF_BOUNDS;
		return QTRACE_ERR;
	}
	*address = hop_addresses[count];
	*latency = hop_latencies[count];
    return QTRACE_OK;
}

unsigned int quicktrace::get_hop_address(int count) {
	if((count < 0) && (count >= (int)hop_count)) {
		result_code = QTRACE_ERR_INDEX_OUT_OF_BOUNDS;
		return INADDR_NONE;
	}
    return hop_addresses[count];
}


double quicktrace::get_hop_latency(int count) {
	if((count < 0) && (count >= (int)hop_count)) {
		result_code = QTRACE_ERR_INDEX_OUT_OF_BOUNDS;
		return -1;
	}
    return hop_latencies[count];
}
