#include "quicktrace.h"
//Common includes
#include <string.h>
#include <stdio.h>
#include <iostream>

static void debug_print_err(void);

#define USE_GET_PATH 0

int main(int argc, char *argv[]) {
    int ret = 0;
    const char *dest;
    int max_hops = 32;
    int reps = 3;
    quicktrace qt;

    if(argc > 1) {
        dest = argv[1];
        if(!strcmp("-h", dest))
            goto QTRACE_USAGE;
        if(argc > 2) {
            for(int i = 2; i < argc; i++) {
                if(!strcmp("-m", argv[i])) {
                    if(++i >= argc) goto QTRACE_USAGE;
                    max_hops = atoi(argv[i]);
                }
                else if(!strcmp("-q", argv[i])) {
                    if(++i >= argc) goto QTRACE_USAGE;
                    reps = atoi(argv[i]);
                }
                else if(!strcmp("-t", argv[i])) {
                    if(++i >= argc) goto QTRACE_USAGE;
                    qt.set_timeout_ms(atoi(argv[i]));
                }                else if(!strcmp("-z", argv[i])) {
                    if(++i >= argc) goto QTRACE_USAGE;
                    qt.set_probe_interval_ms(atoi(argv[i]));
                }
                else if(!strcmp("--seq", argv[i])) {
                    qt.set_sequential_trace(1);
                }
                else if(!strcmp("-h", argv[i])) {
                    goto QTRACE_USAGE;
                }
                else {
                    goto QTRACE_USAGE;
                }
            }
        }
    }
    else {
QTRACE_USAGE:
        std::cout<<"Usage: qtrace <dest> [-m max-ttl] [-q probes-per-hop] [-z pause-ms] [-t timeout-ms] [--seq]"<<std::endl;
        return -1;
    }

    //cout<<"\nTracing from "<<inet_ntoa(*( struct in_addr *)&src_addr);
    cout<<"Tracing to "<<dest<<"... "<<flush;

    double tstart = quicktrace::get_time_msec();
        
    ret = qt.trace(dest, max_hops, reps);
    
    double tstop = quicktrace::get_time_msec();

    sockaddr_in addr;
    addr.sin_addr.s_addr = qt.get_target_address();
    cout<<"\nResolved to "<<inet_ntoa(addr.sin_addr)<<" "<<flush;

    if(ret != QTRACE_OK) {
        debug_print_err();
        cout<<"\nTrace unsuccessful - "<<qt.get_result_message()<<std::endl;
        return -1;
    }
    else {
#if USE_GET_PATH
        vector<unsigned int> addresses;
        vector<double>       latencies;
        qt.get_path(addresses, latencies);
        int hop_count =  addresses.size();
        for(int i = 0; i < hop_count; i++) {
            addr.sin_addr.s_addr = addresses[i];
            char *str_addr = inet_ntoa(addr.sin_addr);
            cout<<"\n"<<((i+1 < 10) ? " " : "" )<<(i+1)
                <<" "<<str_addr<<((const char*)(("          ") + (strlen(str_addr) - 7)))
                <<"\t"<<latencies[i]<<" ms";
        }
#else
        int hop_count = qt.get_hop_count();
        for(int i = 0; i < hop_count; i++) {
            addr.sin_addr.s_addr = qt.get_hop_address(i);
            char *str_addr = inet_ntoa(addr.sin_addr);
            cout<<"\n"<<((i+1 < 10) ? " " : "" )<<(i+1)
                <<" "<<str_addr<<((const char*)(("          ") + (strlen(str_addr) - 7)))
                <<"\t"<<qt.get_hop_latency(i)<<" ms";
        }
#endif
    }
    if(qt.get_result_code() != QTRACE_OK) {
        perror("qtrace error:");
        return -1;
    } 

    cout<<"\nPong="<<qt.pong_recvd()<<", host reached="<<qt.host_reached();
    cout<<"\nDone in "<<(tstop - tstart)<<" ms: "
        <<qt.get_result_code()<<":"
        <<qt.get_result_message()<<endl<<flush;

    return 0;
}

static void debug_print_err(void) {
#if defined _WIN32 || defined _WIN64
    char *error;
    switch(WSAGetLastError()) {
        case 10004: error = "Interrupted system call"; break;
        case 10009: error = "Bad file number"; break;
        case 10013: error = "Permission denied"; break;
        case 10014: error = "Bad address"; break;
        case 10022: error = "Invalid argument (not bind)"; break;
        case 10024: error = "Too many open files"; break;
        case 10035: error = "Operation would block"; break;
        case 10036: error = "Operation now in progress"; break;
        case 10037: error = "Operation already in progress"; break;
        case 10038: error = "Socket operation on non-socket"; break;
        case 10039: error = "Destination address required"; break;
        case 10040: error = "Message too long"; break;
        case 10041: error = "Protocol wrong type for socket"; break;
        case 10042: error = "Bad protocol option"; break;
        case 10043: error = "Protocol not supported"; break;
        case 10044: error = "Socket type not supported"; break;
        case 10045: error = "Operation not supported on socket"; break;
        case 10046: error = "Protocol family not supported"; break;
        case 10047: error = "Address family not supported by protocol family"; break;
        case 10048: error = "Address already in use"; break;
        case 10049: error = "Can't assign requested address"; break;
        case 10050: error = "Network is down"; break;
        case 10051: error = "Network is unreachable"; break;
        case 10052: error = "Net dropped connection or reset"; break;
        case 10053: error = "Software caused connection abort"; break;
        case 10054: error = "Connection reset by peer"; break;
        case 10055: error = "No buffer space available"; break;
        case 10056: error = "Socket is already connected"; break;
        case 10057: error = "Socket is not connected"; break;
        case 10058: error = "Can't send after socket shutdown"; break;
        case 10059: error = "Too many references, can't splice"; break;
        case 10060: error = "Connection timed out"; break;
        case 10061: error = "Connection refused"; break;
        case 10062: error = "Too many levels of symbolic links"; break;
        case 10063: error = "File name too long"; break;
        case 10064: error = "Host is down"; break;
        case 10065: error = "No Route to Host"; break;
        case 10066: error = "Directory not empty"; break;
        case 10067: error = "Too many processes"; break;
        case 10068: error = "Too many users"; break;
        case 10069: error = "Disc Quota Exceeded"; break;
        case 10070: error = "Stale NFS file handle"; break;
        case 10091: error = "Network SubSystem is unavailable"; break;
        case 10092: error = "WINSOCK DLL Version out of range"; break;
        case 10093: error = "Successful WSASTARTUP not yet performed"; break;
        case 10071: error = "Too many levels of remote in path"; break;
        case 11001: error = "Host not found"; break;
        case 11002: error = "Non-Authoritative Host not found"; break;
        case 11003: error = "Non-Recoverable errors: FORMERR, REFUSED, NOTIMP"; break;
        case 11004: error = "Valid name, no data record of requested type"; break;
        default: error = strerror(errno); break;
    }
    cerr<<"\nError "<<WSAGetLastError()<<": "<<error<<flush;
#else 
    perror("\nError");
#endif
    return;
}
