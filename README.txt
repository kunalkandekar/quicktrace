                            QuickTrace                                          
                            ----------

                          Kunal Kandekar 
                       kunalkandekar@gmail.com

The fastest traceroute in the west*! Tired of waiting dozens of seconds - or 
even minutes - for a traceroute, when a ping to the same IP takes mere milli-
seconds? Think it's absurd that a few non-responsive intermediate hops should 
hold everything else up? Just need to get a quick idea what the IP route looks
like? Well, look no further! This code will get you a (mostly) complete route to
a given IP address in about the time it used to take you to send a couple of 
pings! Why, it's so fast, it oughta be called "raceroute"!

Tested on MacOS X (10.6.8) and Linux (Ubuntu 10.10).

* As far as I know**.

** I don't know much.


WORKING
-------
This is a C++ class and command line tool for running quick traceroutes, 
discovering intermediate hop IPs as quickly as possible, at the expense of 
potentially less accurate per-hop RTTs.

It minimizes delay by sending TTL-limited packets (currently UDP) in quick 
succession without waiting for responses, and listening for ICMP response 
packets using without blocking using select. Timestamps are recorded while 
sending and receiving packets, and approximate RTTs are calculated as the 
difference in timestamps for a given TTL. Additionally, ping (ICMP echo request)
packets are also sent to the destination IP, and the total hop count is 
estimated from the TTL of the corresponding ICMP echo reply ("pong"). Any inter-
mediate hops that fail to respond within the timeout are assumed to be non-
responding and dis-regarded (these show up with IPs 0.0.0.0 and RTTs of -1 in 
the results.)


BUILD
-----
Run "make" to build on *nix. Still haven't tested on Windows, but would 
eventually provide e.g. a VS proj file for a Windows build. The code may or may 
not build on Win*. Although this was originally developed and tested on Win32
(Windows ME, can you believe it), subsequent changes may have broken it for 
Win* builds.


USAGE
-----
qtrace <dest-ip> [max-hops] [reps-per-hop] [timeout-ms]

If you think a few hops are incorrectly missing from the resulting path, just 
try running it again! Heck, run it 3 more times! It only takes a second or so
anyway.


ORIGIN
------
Of course, since it returns much less information and is less reliable than your
average traceroute, it's probably useful for much fewer use cases. However, 
there is a reason this code does only what it does, and its speed is only a 
(very useful) side-effect.

This was implemented for work related to my Master's thesis on P2P networks, 
which was pretty cool, if I do say so myself. We discovered that with knowledge 
of only the intermediate hop IP addresses and end-to-end latencies to a small 
(about 5) set of "landmark" nodes, peers joining anywhere in a P2P network 
could be efficiently routed through the overlay to their nearest IP network 
neighbor with over 95% accuracy using a completely distributed algorithm. Per-
hop latencies and DNS hostnames of intermediate hops were nice to know, but 
didn't provide any further improvements.

Since only the intermediate IPs and end-to-end pings were sufficient, the native 
traceroute utility provided by all OS's was too slow and provided way more 
information than was needed. Hence, this was developed as an alternative method 
to quickly discover only the information that was useful to us.


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
-------
Tracing to ncsu.edu... 
Resolved to 152.1.226.10 
 1 192.168.1.1      	4.05298 ms
 2 69.142.0.1       	23.95 ms
 3 68.85.118.161    	11.6479 ms
 4 68.85.62.17      	17.741 ms
 5 68.86.95.177     	17.0781 ms
 6 68.86.87.226     	15.4812 ms
 7 173.167.56.146   	15.2861 ms
 8 64.57.20.196     	22.2561 ms
 9 64.57.21.226     	31.573 ms
10 128.109.9.2      	28.5081 ms
11 128.109.9.17     	33.02 ms
12 128.109.248.62   	41.231 ms
13 152.1.6.249      	35.9141 ms
14 152.1.6.226      	36.0679 ms
15 152.1.226.10     	39.2009 ms
Pong=1, host reached=1
Done in 793.106 ms: 0:OK


Example with unresponsive intermediate hops:
--------------------------------------------
(Note the 0.0.0.0 IPs with -1 ms RTTs. Also, the total time taken is much higher
since, the code waits the entire timeout to give non-responsive routers a chance
to respond.)

Tracing to google.com... 
Resolved to 74.125.226.225 
 1 192.168.1.1      	1.57007 ms
 2 69.142.0.1       	26.1599 ms
 3 68.85.118.161    	9.96704 ms
 4 68.85.62.17      	13.9541 ms
 5 68.86.95.181     	18.323 ms
 6 68.86.86.194     	24.636 ms
 7 0.0.0.0          	-1 ms
 8 0.0.0.0          	-1 ms
 9 0.0.0.0          	-1 ms
10 74.125.226.225   	14.9292 ms
Pong=1, host reached=0
Done in 2235.05 ms: 0:OK
