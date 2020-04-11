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
                }
                else if(!strcmp("-z", argv[i])) {
                    if(++i >= argc) goto QTRACE_USAGE;
                    qt.set_probe_interval_ms(atoi(argv[i]));
                }
                else if(!strcmp("-v", argv[i])) {
                    if(++i >= argc) goto QTRACE_USAGE;
                    qt.set_verbosity(atoi(argv[i]));
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
        std::cout<<"Usage: qtrace <dest> [-m max-ttl] [-q probes-per-hop] [-z pause-ms] [-t timeout-ms] "
                <<"[-v verbosity-level] [--seq]"<<std::endl;
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
    cerr<<"\nError "<<quicktrace::last_error_msg()<<std::endl;
    return;
}
