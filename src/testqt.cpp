#include "quicktrace.h"
static void debug_print_err(void);

int main(int argc, char *argv[]) {
	int ret = 0;
	char *dest;
	int max_hops = 32;
	int reps = 1;
	quicktrace qt;

	if(argc > 1) {
		dest = argv[1];
		if(argc > 2) {
			max_hops = atoi(argv[2]);
			if(argc > 3) {
				reps = atoi(argv[3]);
				if(argc > 4) {
				    qt.set_timeout_ms(atoi(argv[4]));
				    if(argc > 5) {
					   qt.set_sequential_trace(1);
				    }
				}
			}
		}
	}
	else {
		dest = (char*)"152.1.226.10"; //ncsu.edu
	}
	//cout<<"\nTracing from "<<inet_ntoa(*( struct in_addr *)&src_addr);
	cout<<"\nTracing to "<<dest<<"... "<<flush;
	double ms = 0;
#ifdef WIN32
	LARGE_INTEGER start;
	LARGE_INTEGER stop;
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);
#else
	struct timeval tv_start;
	struct timeval tv_stop;
	gettimeofday(&tv_start, 0);
#endif
	
	ret = qt.trace(dest, max_hops, reps);
	unsigned int dst = qt.get_target_address();
	cout<<"\nResolved to "<<inet_ntoa(*(struct in_addr*)&dst)<<" "<<flush;

#ifdef WIN32
	QueryPerformanceCounter(&stop);
	ms = (double(stop.QuadPart - start.QuadPart)/double(freq.QuadPart))*1000;
#else
	gettimeofday(&tv_stop, 0);
	ms = (tv_stop.tv_sec - tv_start.tv_sec) *1000 + double(tv_stop.tv_usec - tv_start.tv_usec)/1000;
#endif
	
	if(ret != QTRACE_OK) {
		debug_print_err();
	}
	else {
		int hop_count = qt.get_hop_count();
		unsigned int addr;
		char *str_addr;
		for(int i = 0; i < hop_count; i++) {
			addr = qt.get_hop_address(i);
			str_addr = inet_ntoa(*(struct in_addr*)&addr);
			cout<<"\n"<<((i+1 < 10) ? " " : "" )<<(i+1)
				<<" "<<str_addr<<((const char*)(("          ") + (strlen(str_addr) - 7)))
				<<"\t"<<qt.get_hop_latency(i)<<" ms";
		}
	}
	cout<<"\nDone in "<<ms<<" ms: "
		<<qt.get_result_code()<<":"
		<<qt.get_result_message()<<endl<<flush;
	if(qt.get_result_code() != QTRACE_OK) {
		perror("qtrace error:");
	} 
	return 0;
}

static void debug_print_err(void) {
#ifdef WIN32
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
