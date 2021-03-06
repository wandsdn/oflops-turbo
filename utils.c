#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include <arpa/inet.h>

#include <net/ethernet.h>
#include <net/if_arp.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>

#include <net/if.h>
#include <sys/ioctl.h>

#include <sys/time.h>

#include "utils.h"

/****************************************************************
 * shouldn't this be a in libc?  I mean... come on...
 */

void * _realloc_and_check(void * ptr, size_t bytes, char * file, int lineno)
{
    void * ret = realloc(ptr,bytes);
    if(!ret)
    {
        perror("malloc/realloc: ");
        // use fprintf here in addition to flowvisor_err, incase we can't allocate the err msg buf
        fprintf(stderr, "Malloc/Realloc(%zu bytes) failed at %s:%d\n",bytes,file,lineno);
        abort();
    }
    return ret;
}


/***************************************************************
 * print errno and exit
 */

void perror_and_exit(const char * str, int exit_code)
{
    perror(str);
    exit(exit_code);
}

void set_timeval(struct timeval *target, struct timeval *val)
{
    target->tv_sec = val->tv_sec;
    target->tv_usec = val->tv_usec;
}

void add_time(struct timeval *now, time_t secs,  suseconds_t usecs)
{
    const uint64_t sec_to_usec = 1000000;
    now->tv_sec += secs;
    now->tv_usec += usecs;
    if(now->tv_usec >= sec_to_usec) {
        now->tv_sec += 1;
        now->tv_usec -= sec_to_usec;
    }
}

void add_timespec(struct timespec *now, time_t secs, long nsecs)
{
    const uint64_t sec_to_nsec = 1000000000;
    now->tv_sec += secs;
    now->tv_nsec += nsecs;
    if(now->tv_nsec >= sec_to_nsec) {
        now->tv_sec += 1;
        now->tv_nsec -= sec_to_nsec;
    }
}

uint32_t
time_diff(struct timeval *now, struct timeval *then) {
    return (then->tv_sec - now->tv_sec)*1000000 + (then->tv_usec - now->tv_usec);
}

int64_t timespec_diff(struct timespec *now, struct timespec *then) {
    return (then->tv_sec - now->tv_sec)*INT64_C(1000000000) + (int64_t)(then->tv_nsec - now->tv_nsec);
}

int
time_cmp(struct timeval *now, struct timeval *then) {
    if(then->tv_sec != now->tv_sec) {
        return (then->tv_sec < now->tv_sec)?-1:1;
    } else if (then->tv_usec != now->tv_usec) {
        return (then->tv_usec < now->tv_usec)?-1:1;
    } else
        return 0;
}

void* xmalloc(size_t len)
{
    void *p = NULL;
    p = malloc(len);
    if (p == NULL)
        fail("Failed while allocating memmory");
    return p;
}

void fail(const char * msg) {
    printf("error: %s\n", msg);
    exit(1);
}

/*
   uint64_t
   ntohll(uint64_t val) {
   uint64_t ret = 0;

   ret=((val & 0x00000000000000FF) << 56) |
   ((val & 0x000000000000FF00) << 40) |
   ((val & 0x0000000000FF0000) << 24) |
   ((val & 0x00000000FF000000) << 8)  |
   ((val & 0x000000FF00000000) >> 8)  |
   ((val & 0x0000FF0000000000) >> 24) |
   ((val & 0x00FF000000000000) >> 40) |
   ((val & 0xFF00000000000000) >> 56);

   return ret;
   }
   */

uint16_t ip_sum_calc(uint16_t len , uint16_t ip[]) {

    uint32_t sum = 0;  /* assume 32 bit long, 16 bit short */


    while(len > 1){
        sum += htons(*((uint16_t *) ip));
        ip++;
        if(sum & 0x80000000)   /* if high order bit set, fold */
            sum = (sum & 0xFFFF) + (sum >> 16);
        len -= 2;
    }

    if(len)       /* take care of left over byte */
        sum += (unsigned short) *(unsigned char *)ip;

    while(sum>>16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ~sum;
}

int get_mac_address(char *intf_name, char *mac_addr)
{
    struct ifreq ifr;
    int s;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s==-1) {
        return -1;
    }

    memset(&ifr, 0x00, sizeof(ifr));

    strcpy(ifr.ifr_name, intf_name);
    if(ioctl(s, SIOCGIFHWADDR, &ifr) != 0) {
        perror_and_exit("ioctl", 1);
    }

    close(s);

    memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN * sizeof(char));
    return 0;
}

void hexdump(const uint8_t *data, uint32_t len)
{
    int ix;
    for (ix=0;ix<len;ix++){
        if(ix>0 && (ix%16 == 0)) printf("\n");
        else if(ix>0 && (ix%8 == 0)) printf(" ");
        printf("%02x", data[ix]);
    }
    return;
}
