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
pings! Why, it's so durn fast, it oughta be called "Raceroute"!

Tested on MacOS X (10.6.8), Linux (Ubuntu 10.10) and Windows 7 (Enterprise, 
with Service Pack 1).

* As far as I know**.

** I don't know much.


WORKING
-------
This is a C++ class and command line tool for running quick traceroutes, 
discovering intermediate hop IPs as quickly as possible, at the expense of 
potentially less accurate per-hop RTTs.

It minimizes delay by sending TTL-limited packets (ICMP ECHO request packets on
Windows, UDP on Linux and Macs) in quick succession without waiting for 
responses, and listening for ICMP response packets using without blocking using 
select. Timestamps are recorded while sending and receiving packets, and 
approximate RTTs are calculated as the difference in timestamps for a given TTL.
Additionally, ping (ICMP echo request) packets are also sent to the destination 
IP, and the final total hop count is estimated from the TTL of the corresponding
ICMP echo reply ("pong"). 

Estimating hop count from the pong TTL is trickier than it seems because 
different OSes send packets with different initial TTLs. Nonetheless QuickTrace 
is able to quickly make an educated guess as to the initial TTL of the packet 
(based on the ranges of TTLs the pong TTL falls in.) However, quite often the 
number of hops on the forward path (i.e. the path taken by the PING we send)
is different from the reverse path (i.e that taken by the ICMP response packet).
In one case the return path was shorter by 5 hops! If QuickTrace assumed the 
paths to be symmetric in this case, it may have stopped probing prematurely.
As such the estimate can be wrong and can lead to QuickTrace giving incorrect
results, so it's not relied upon.

So QuickTrace used another method to quickly estimate number of hops to target.
It detects when host-unreachable ICMP responses are received, and the encoded
information in the return packets provide an estimated upper-bound for the hop 
count from that, since they are sent when the packet reaches the destination 
and nobody is (typically) listening on the destination port. Again, this is 
a heuristic and not a sure-fire method for many reasons. Still, these methods
enable us to quickly estimate the total path hop count. Any intermediate hops 
that fail to respond within the timeout are assumed to be non-responding and 
pre-emptively dis-regarded (these show up with IPs 0.0.0.0 and RTTs of -1 in 
the results.)

Note that final hop count estimation is prone to give incorrect results if:
1) The final hop responds with an unconventional or incorrect initial TTL.
2) QuickTrace guesses the initial TTL incorrectly. 


BUILD
-----
*nix:  Run "make" to build on *nix. 

Windows / VIsual Studio: Open the quicktrace.sln Visual Studio project and 
navigate to BUILD -> Build Solution (or hit the shortcut key, F7). 

Remember to change the build setting in the dropdown in the menu bar to 
configure it for a Release build as opposed to Debug. (Not that it makes a huge
difference.) The freely available Express version for Visual Studio suffices, 
and in fact, that's the one I used.

Although this was originally developed and tested on Win32 (Windows ME, 
can you believe it), subsequent development was on OS X and Linux, so there were
many changes that broke it on Win* builds. Finally fixed, built and tested on 
Windows and provided a Visual Studio project as promised. 


USAGE
-----
qtrace <dest> [-m max-ttl] [-q probes-per-hop] [-z pause-ms] [-t timeout-ms] 
              [-v verbosity-level] [--seq]

If you think a few hops are incorrectly missing from the resulting path, just 
try running it again! Heck, run it 3 more times! It only takes a second or so
anyway.

Note that on Windows, you need administrator privileges to run qtrace.exe, since
it uses raw sockets to receive ICMP packets. (In fact, this will probably only 
run on Windows Vista or later; apparently, XP removed support for raw sockets.)

Similarly, you need admin rights on Linux for the same reasons. 

Only on OS X can you get away without admin rights. This is because they let you
*receive* (but not send) ICMP packets on a datagram socket (by opening it with 
the SOCK_DGRAM and IPPROTO_ICMP parameters). You need admin rights to open raw 
sockets, but as we are *not opening* a raw, we can do without. As I understand 
it, you cannot send ICMP (or raw) packets on that datagram/ICMP socket. However,
since we send UDP packets rather than ICMP, we don't need to send any ICMP or 
raw packets at all, and hence we can eschew raw sockets and the need for admin 
rights totally.

I think it's a rather elegant solution to the whole privileges problem of giving
access to raw sockets (nobody wants to make it easier for compromised boxes to 
do even more damaging stuff, like spoofing), but I can imagine that internally 
it's probably a rather hairy hack.


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


ORIGIN
------
Of course, since it returns much less information and is less reliable than your
average traceroute, it's probably useful for fewer use cases. However, there is 
a reason this code does only what it does, and its speed is only a (very useful)
side-effect.

This was implemented for work related to my Master's thesis on P2P networks, 
which was pretty cool, if I do say so myself. We discovered that with knowledge 
of only the intermediate hop IP addresses and end-to-end latencies to a small 
(about 3 - 5) set of "landmark" nodes, peers joining anywhere in a P2P network 
could be efficiently routed through the overlay to their nearest IP network 
neighbor with over 95% accuracy using a completely distributed algorithm. Per-
hop latencies and DNS hostnames of intermediate hops were nice to know, but 
didn't provide any further improvements.

Since only the intermediate IPs and end-to-end pings were sufficient, the native 
traceroute utility provided by all OS's was too slow and provided way more 
information than was needed. Hence, this was developed as an alternative method 
to quickly discover only the information that was useful to us.


"PRIOR ART"
-----------
So, googling for "fastest traceroute" brought up a couple of other tools that do
something similar to QuickTrace, and apparently have been around for a while. 
However, QuickTrace is still somewhat superior, although narrowly so. Reviewing 
the alternatives, it seems the "unique selling point" for QuickTrace is how it 
intelligently knows when to terminate correctly. As described above, it 
basically does this by sending a ping in parallel with the probe packets, and 
estimating the final hop count based on the TTL of the pong packet. This lets it
terminate much faster and more deterministically, especially if all intermediate
hops also respond correctly. 

Also, the output looks more correct, since the last hop and destination IP 
address are determined from the pong (or host unreachable) packets. For example,
often a target hostname will correctly resolve to a destination IP address, and 
it is to this address that probes and pings are send. However the machine at 
that IP often does not respond to pings/probes (try probing google.com and 
microsoft.com, for example,) and hence traceroute tools do not typically display
that IP as the final hop. This is most probably because they fail to estimate 
the route hop count, and hence, even when they know the destination IP address,
they do not know at which hop to display it.

The earliest implementation I could find for a fast traceroute was here:
Project Argus - Network topology discovery, monitoring, history, and 
visualization.
http://www.cs.cornell.edu/boom/1999sp/projects/network%20topology/topology.html

Another one I found recently is WinMTR. 
http://winmtr.net/
Haven't tried it out, but it seems to do fast traceroute by spawning a separate
thread for each probe packet, thus sending probes in parallel.

Yet another one is here: ftrace - Fast Traceroute for Win32
http://www.r1ch.net/stuff/ftrace/
(Tried it out, and it seems just like plain old traceroute to me…)

Yet another one:
HyperTrace
http://www.analogx.com/contents/download/Network/htrace/Freeware.htm
(This one is at least much faster than the one above.)


WINDOWS vs *NIX / ICMP vs UDP
-----------------------------
Since Windows needs ICMP for probe packets rather than UDP, the code now has two
separate code paths based on the OS it is compiled on. It's an additional 
headache but it cannot be helped.

I always wondered why Windows uses ICMP probes instead of UDP for traceroute. I 
also always wondered why there were no tools that provided a UDP version of 
tracert on Windows. I tried to fill the gap, and based on my attempts to do it 
with UDP, it seems nigh impossible, at least not without access to undocumented 
APIs. A time exceeded ICMP response for a low TTL UDP packet is never received 
by the ICMP raw socket on windows; instead the UDP socket returns an error, and
the error code given is ECONNRESET (or WSAECONNRESET for WinSock.) This error is
actually for "Network dropped connection or reset", which is absurd because UDP
is a connectionless protocol, so dropping or resetting a connection as in TCP is
impossible. Instead, Windows uses this hack, whereby it apparently intercepts 
the ICMP response and instead returns an error to let you know that the TTL on 
your UDP packet expired; but it won't let you do anything else, much less get 
the IP address of the router that sent the ICMP packet.

Anyway, as long as it works, apart from programmer difficuly, probing using ICMP
as opposed to UDP shouldn't have any qualitative differences, right? Not quite.
One main difference I noticed was that after running a few traces rapidly (as 
little as three or more), ICMP Time Exceeded responses to TTL-limited ICMP Echo
requests stop coming in entirely. That is, ICMP Echo request packets are being
sent out by qtrace, and they are getting dropped, but ICMP packets are not being
generated as they should.

This, from what I understand is because many routers in the Internet detect 
rapid ICMP packets being sent, and either throttle them or outright drop those
packets. So, yes, our ICMP echo packets *are* being dropped, but not necessarily
for the right reasons! Not because the TTL expired, but simply because we are
sending too many of them (and potentially because we are assumed to be malicious
entities, up to some mischief sending this deluge of ICMP packets. From what I 
hear Cisco routers are pretty notorious for this.

You can try this by running "qtrace.exe <some-address> -q 1 -z 5" multiple times
in quick succession. You may notice that the runs after the 3rd or 4th one stop
returning correct paths, and in fact often have severely corrupt or even 
completely missing paths. Running it in verbose mode (using the "-v" option) 
reveals that after the first few hops, none of the routers provide any response,
which means one or all of them are silently dropping our ICMP echo requests. I
had to wait several minutes without any quicktrace / traceroute activity before
I was able to get responses for all packets (and hence, correct paths) again.

For this reason, the Windows implementation starts with a higher default inter-
probe interval (100 ms) as opposed to for other platforms (around 10 ms). This
causes quicktrace take longer to send all probes, but does a decent enough job
of avoiding tripping the router's TOO-MANY-ICMP-PACKETS alarms.


ALTERNATE NAMES
---------------
- Raceroute
- QuickRoute

