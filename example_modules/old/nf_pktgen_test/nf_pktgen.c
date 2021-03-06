#include <sys/queue.h>
#include <math.h>

//include gsl to implement statistical functionalities
#include <gsl/gsl_statistics.h>
 
#include "context.h"
#include "utils.h"
#include "log.h"
#include "traffic_generator.h"

#define BYESTR "bye bye"

/** @ingroup modules
 * queue delay module.
 * This module send a a single packet probe in 
 * order define at which rate  packet buffering ,ay appear.
 *
 * Copyright (C) Computer Laboratory, University of Cambridge, 2011
 * @author crotsos
 * @date February, 2011
 * 
 * @return name of module */
char * name() {
	return "snmp_queue_delay";
}

/** Some constants to help me with conversions
 */
const uint64_t sec_to_usec = 1000000;
const uint64_t byte_to_bits = 8, mbits_to_bits = 1024*1024;

/** packet size limits
 */
#define MIN_PKT_SIZE 64
#define MAX_PKT_SIZE 1500

/** Send sequence
 */
uint32_t sendno;

char local_mac[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
int finished;
int pkt_size = 1500;
int rate = 10;

struct entry {
  struct timeval snd,rcv;
  int ch, id;
  TAILQ_ENTRY(entry) entries;         /* Tail queue. */
}; 

/** The rate at which data will be send between the data ports (In Mbits per sec.). 
 */
uint64_t duration = 60;

TAILQ_HEAD(tailhead, entry) head;

FILE *measure_output;
double *delay;
uint32_t delay_count;

int init(struct oflops_context *ctx, char * config_str) {
  char *pos = NULL;
  char *param = config_str;
  char *value = NULL;

  //init counters
  sendno = 0;
  TAILQ_INIT(&head);
  finished = 0;
  //open file for storing measurement
  measure_output = fopen("measure.log", "w");

  while(*config_str == ' ') {
    config_str++;
  }
  param = config_str;
  while(1) {
    pos = index(param, ' ');

    if((pos == NULL)) {
      if (*param != '\0') {
        pos = param + strlen(param) + 1;
      } else
        break;
    }
    *pos='\0';
    pos++;
    value = index(param,'=');
    *value = '\0';
    value++;
    if(value != NULL) {
      if(strcmp(param, "pkt_size") == 0) {
        pkt_size = atoi(value);
        if((pkt_size <= 70) || (pkt_size > 1500))  
          perror_and_exit("Invalid pkt size param(Values between 70 and 1500 bytes)", 1);
        
      } else if(strcmp(param, "duration") == 0) {
        duration = (uint64_t)atoi(value);
	if((duration < 10) )  
          perror_and_exit("Invalid duration param(Values larger than 10 sec)", 1);
        
      } else if(strcmp(param, "rate") == 0) {
        rate = (uint64_t)atoi(value);        
      }
      param = pos;
    }
  } 
  return 0;
}

/** Initialization
 * @param ctx pointer to opaque context
 */
int start(struct oflops_context * ctx) { 
  void *b;
  struct flow *fl = (struct flow*)xmalloc(sizeof(struct flow));
  int ret, res, len, i;
  struct timeval now;
  msg_init();  

  //get the mac address of channel 1
  get_mac_address(ctx->channels[OFLOPS_DATA1].dev, local_mac);
  printf("%s: %02x:%02x:%02x:%02x:%02x:%02x\n", ctx->channels[OFLOPS_DATA1].dev,
	 (unsigned char)local_mac[0], (unsigned char)local_mac[1], 
	 (unsigned char)local_mac[2], (unsigned char)local_mac[3], 
	 (unsigned char)local_mac[4], (unsigned char)local_mac[5]);


  make_ofp_hello(&b);
  ret = write(ctx->control_fd, b, sizeof(struct ofp_hello));
  free(b);  

  // send a features request, to stave off timeout (ignore response)
  printf("cleaning up flow table...\n");
  res = make_ofp_flow_del(&b);
  ret = write(ctx->control_fd, b, res);
  free(b);

  //Send a singe ruke to route the traffic we will generate
  bzero(fl, sizeof(struct flow));
  fl->mask = OFPFW_IN_PORT | OFPFW_TP_DST; 
  fl->in_port = htons(ctx->channels[OFLOPS_DATA1].of_port); 
  fl->dl_type = htons(ETHERTYPE_IP); 
  memcpy(fl->dl_src, local_mac, ETH_ALEN); 
  memcpy(fl->dl_dst, "\x00\x1e\x68\x9a\xc5\x75", ETH_ALEN); 
  fl->dl_vlan = 0xffff;
  fl->nw_proto = IPPROTO_UDP;
  fl->nw_src =  inet_addr("10.1.1.1");
  fl->nw_dst =  inet_addr("10.1.1.2");
  fl->tp_src = htons(8080);            
  fl->tp_dst = htons(8080); 

  len = make_ofp_flow_add(&b, fl, OFPP_IN_PORT, 0, OFP_FLOW_PERMANENT);
  write(ctx->control_fd, b, len);
  free(b);

  fl->in_port = htons(ctx->channels[OFLOPS_DATA2].of_port); 
  fl->nw_dst =  inet_addr("192.168.1.0");
  for (i = 0 ; i <= 11; i++) {
    len = make_ofp_flow_add(&b, fl, OFPP_IN_PORT, 0, OFP_FLOW_PERMANENT);
    write(ctx->control_fd, b, len);
    free(b);
    fl->nw_dst = htonl(ntohl(fl->nw_dst) + 1);
    //    fl->nw_dst =  inet_addr("10.1.1.2");
  }

  free(fl);

  //end process 
  gettimeofday(&now, NULL);
  add_time(&now, 60, 0);
  oflops_schedule_timer_event(ctx,&now, BYESTR);

  return 0;
}

int handle_timer_event(struct oflops_context * ctx, struct timer_event *te) {  
  //terminate process 
  if (strcmp(te->arg, BYESTR) == 0) {
    printf("terminating test....\n");
    oflops_end_test(ctx,1);
    return 0;    
  }
}

/** Register pcap filter.
 * @param ctx pointer to opaque context
 * @param ofc enumeration of channel that filter is being asked for
 * @param filter filter string for pcap
 * @param buflen length of buffer
 */
int get_pcap_filter(struct oflops_context *ctx, oflops_channel_name ofc, char * filter, int buflen) {
  //set the system to listwn on port 1
  if( (ofc == OFLOPS_DATA1) || (ofc == OFLOPS_DATA2))
    return snprintf(filter,buflen," ");
  else 
    return 0;
  
}

int 
handle_pcap_event(struct oflops_context *ctx, struct pcap_event * pe, oflops_channel_name ch) {
  struct pktgen_hdr *pktgen;
  struct in_addr in;

  if( (ch == OFLOPS_DATA1) || (ch == OFLOPS_DATA2)){  

    struct flow fl; 
    pktgen = extract_pktgen_pkt(ctx, ch, pe->data, pe->pcaphdr.caplen, &fl); 
    in.s_addr =  fl.nw_dst;
    printf("%u.%06u Packet received %s!\n", (uint32_t)pe->pcaphdr.ts.tv_sec, 
	   (uint32_t)pe->pcaphdr.ts.tv_usec, inet_ntoa(in)); 

    
    
/*    /\*  if(htonl(pktgen->seq_num) % 100000 == 0) *\/ */
/* /\*       printf("data packet received %d\n", htonl(pktgen->seq_num)); *\/ */
    
/*     snd.tv_sec = htonl(pktgen->tv_sec); */
/*     snd.tv_usec = htonl(pktgen->tv_usec); */
/*     memcpy(&rcv, &pe->pcaphdr.ts, sizeof(struct timeval)); */
/*     delay[delay_count++] = (double)time_diff(&snd, &rcv); */
  } 
  return 0;
}

int
handle_traffic_generation (oflops_context *ctx) {
  struct traf_gen_det det;
  uint64_t data_snd_interval;

  char mac[20];

  init_traf_gen(ctx);


  snprintf(mac, 20, "%02x:%02x:%02x:%02x:%02x:%02x",
	   (unsigned char)local_mac[0], (unsigned char)local_mac[1], 
	   (unsigned char)local_mac[2], (unsigned char)local_mac[3], 
	   (unsigned char)local_mac[4], (unsigned char)local_mac[5]);
  
  //background data
  strcpy(det.src_ip,"10.1.1.1");
  strcpy(det.dst_ip_min,"10.1.1.2");
  strcpy(det.dst_ip_max, "10.1.1.2");
  
  struct in_addr in;
/*   in.s_addr = htonl(ntohl(inet_addr("10.1.1.2")) + 10); */
/*   strcpy(det.dst_ip_max,  inet_ntoa(in)); */
  
  if(ctx->trafficGen == PKTGEN)
    strcpy(det.mac_src, "00:00:00:00:00:00");
  else
    strcpy(det.mac_src, mac);  //"00:1e:68:9a:c5:74");
  strcpy(det.mac_dst,"00:1e:68:9a:c5:75");
  det.vlan = 0xffff;
  det.vlan_p = 1;
  det.vlan_cfi = 0;
  det.udp_src_port = 8080;
  det.udp_dst_port = 8080;
  det.pkt_size = pkt_size;
  strcpy(det.flags, "IPDST_RND");
    
  //claculate interpacket gap
  data_snd_interval = (pkt_size * byte_to_bits * sec_to_usec*1000) / (rate * mbits_to_bits);
  det.delay = data_snd_interval;
  
  //print sending probe details
  fprintf(stderr, "Sending data interval : %u nsec (pkt_size: %u bytes, rate: %u Mbits/sec %llu packets)\n", 
	  (uint32_t)data_snd_interval, (uint32_t)pkt_size, (uint32_t)rate,  det.pkt_count);
  
  //start packet generator
  add_traffic_generator(ctx, OFLOPS_DATA1, &det);

  strcpy(det.src_ip,"10.1.1.1");
  strcpy(det.dst_ip_min,"192.168.1.0");
  in.s_addr = htonl(ntohl(inet_addr("192.168.1.0")) + 10);
  strcpy(det.dst_ip_max,  inet_ntoa(in));
  add_traffic_generator(ctx, OFLOPS_DATA2, &det);

  start_traffic_generator(ctx);

  
  
/*   gsl_sort (delay, 1, delay_count); */
/*   mean = (uint32_t)gsl_stats_mean(delay, 1, delay_count); */
/*   std = (uint32_t)sqrt(gsl_stats_variance(delay, 1, delay_count)); */
/*   median = (uint32_t)gsl_stats_median_from_sorted_data (delay, 1, delay_count); */
/*   loss = (float)delay_count/(float)det.pkt_count; */
/*   printf("delay:%d:%u:%u:%u:%.4f:%d\n",  */
/* 	 datarate[i], mean, median, std, loss, delay_count); */
/*   snprintf(msg, 1024, "delay:%d:%u:%u:%u:%.4f:%d",  */
/* 	   datarate[i], mean, median, std, loss, delay_count); */
/*   gettimeofday(&now, NULL); */
/*   oflops_log(now, GENERIC_MSG, msg); */

  oflops_end_test(ctx,1);
  return 1;
}

int 
of_event_echo_request(struct oflops_context *ctx, const struct ofp_header * ofph) {
  void *b;
  int res;
  make_ofp_hello(&b);
  ((struct ofp_header *)b)->type = OFPT_ECHO_REPLY;
  ((struct ofp_header *)b)->xid = ofph->xid;
  res = oflops_send_of_mesgs(ctx, b, sizeof(struct ofp_hello));
  free(b);
  return 0;
}
