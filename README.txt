QuickTrace
----------
C++ class and command line tool for running quick traceroutes, discovering 
intermediate hop IPs as quickly as possible, at the expense of less accurate 
per-hop RTTs.

Example C++ source (testqt.cpp):
------------------------------
#include "quicktrace.h"
#include <iostream>
using namespace std;
int main(int argc, char *argv[]) {
    char *dest = "ncsu.edu";
    quicktrace qt;    int ret = qt.trace(dest, 32, 1);   //max 32 hops, 1 packet per hop
    unsigned int dst = qt.get_target_address();
    cout<<"\nResolved "<<dest<<" to "<<inet_ntoa(*(struct in_addr*)&dst)<<"\n"<<flush;
    
    if(ret == QTRACE_OK) {
        int hop_count = qt.get_hop_count();
        unsigned int addr;
        char *str_addr;
        for(int i = 0; i < hop_count; i++) {
            addr = qt.get_hop_address(i);
            str_addr = inet_ntoa(*(struct in_addr*)&addr);
            cout<<"\t"<<((i+1 < 10) ? " " : "" )<<(i+1)
                <<" "<<str_addr<<((const char*)(("          ") + (strlen(str_addr) - 7)))
                <<"\t"<<qt.get_hop_latency(i)<<" ms\n";
        }
    }
    return 0;
}

Output:
------
Resolved ncsu.edu to 152.1.226.10
	 1 192.168.1.1      	9.02808 ms
	 2 69.142.0.1       	43.905 ms
	 3 68.85.118.161    	23.8062 ms
	 4 68.85.63.9       	25.5891 ms
	 5 68.86.92.217     	32.4128 ms
	 6 68.86.87.226     	32.084 ms
	 7 173.167.56.146   	29.5911 ms
	 8 64.57.20.196     	32.9109 ms
	 9 64.57.21.226     	40.7249 ms
	10 128.109.9.2      	39.5239 ms
	11 128.109.9.17     	43.114 ms
	12 128.109.248.62   	62.6279 ms
	13 152.1.6.249      	63.405 ms
	14 152.1.6.226      	64.0051 ms
	15 152.1.226.10     	63.458 ms
	16 152.1.226.10     	63.4431 ms
	17 152.1.226.10     	63.4092 ms
	18 152.1.226.10     	63.4561 ms
	19 152.1.226.10     	63.53 ms
	20 0.0.0.0          	-1 ms
	21 152.1.226.10     	63.5808 ms