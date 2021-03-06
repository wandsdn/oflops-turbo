
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <pcap.h>

#include "oflops.h"
#include "usage.h"
#include "control.h"
#include "context.h"
#include "module_run.h"
#include "log.h"
#include "signal.h"
#include "traffic_generator.h"


void * 
run_module(void *param) {
  struct run_module_param* tmp = (struct run_module_param *)param;
  return (void *)run_test_module(tmp->ctx, tmp->ix_mod);
}

void *
start_traffic_thread(void *param) {
  struct run_module_param* tmp = (struct run_module_param *)param;
  return (void *)run_traffic_generation(tmp->ctx, tmp->ix_mod);

}

int main(int argc, char * argv[])
{
  int i, j;
  struct pcap_stat ps;
  pthread_t thread, event_thread, traffic_gen;
  struct run_module_param *param = malloc_and_check(sizeof(struct run_module_param));
  char msg[1024];
  struct timeval now;
  struct nf_cap_stats stat;
  struct nf_gen_stats gen_stat;

  // create the default context
  oflops_context * ctx = oflops_default_context();
  param->ctx = ctx;
  parse_args(ctx, argc, argv);

  if(ctx->n_tests == 0 )
    usage("Need to specify at least one module to run\n",NULL);

  oflops_log_init(ctx->log);

  fprintf(stderr, "Running %d Test%s\n", ctx->n_tests, ctx->n_tests>1?"s":"");

  for(i=0;i<ctx->n_tests;i++) {
    // init contaxt and setup module
    fprintf(stderr, "-----------------------------------------------\n");
    fprintf(stderr, "------------ TEST %s ----------\n", (*(ctx->tests[i]->name))());
    fprintf(stderr, "-----------------------------------------------\n");
    reset_context(ctx);
    ctx->curr_test = ctx->tests[i];
    ctx->traffic_gen = &traffic_gen;
    ctx->started = 0;
    param->ix_mod = i;

    setup_test_module(ctx,i);
    //start all the required threads of the program

    // the data receiving thread
    pthread_create(&thread, NULL, run_module, (void *)param);
    // Now start up the control channel
    setup_control_channel(ctx);
    ctx->curr_test->start(ctx);
    ctx->started = 1;

    // the data generating thread
    pthread_create(&traffic_gen, NULL, start_traffic_thread, (void *)param);
    // the timer thread.
    pthread_create(&event_thread, NULL, event_loop, (void *)param);

    /* First wait for traffic generation to stop */
    pthread_join(traffic_gen, NULL);
    /* now stop timers etc */
    ctx->end_event = 1;
    pthread_join(event_thread, NULL);
    usleep(1000000);// Sleep for 1000ms to give time to flush the OF channel
    if (ctx->fluid_control)
        teardown_control_channel(ctx);
    /* Now end the module */
    usleep(1000000);// Sleep for 1000ms to give time to flush to the PCAP file
    ctx->end_module = 1;
    pthread_join(thread, NULL);

    //reading details for the data generation and capture process and output them to the log file.
    gettimeofday(&now, NULL);
    for(j = 0 ; j < ctx->n_channels;j++) {
      if((ctx->channels[j].cap_type == PCAP) && 
          (ctx->channels[j].pcap_handle != NULL)) {
        pcap_stats(ctx->channels[j].pcap_handle, &ps);
        snprintf(msg, 1024, "%s:%u:%u",ctx->channels[j].dev, ps.ps_recv, ps.ps_drop);
        oflops_log(now, PCAP_MSG, msg);
        printf("%s\n", msg);

        // FIXME: this requires a parsing code to extract only required information and not the whole string. 
        char *ret = report_traffic_generator(ctx);
        if(ret) {
          oflops_log(now, PKTGEN_MSG, report_traffic_generator(ctx));
          printf("%s\n", ret);
        }

      } else if((ctx->channels[j].cap_type == NF2) &&
          (ctx->channels[j].nf_cap != NULL)) {
        nf_cap_stat(j-1, &stat);
        snprintf(msg, 1024, "%s:rcv:%u:%u",ctx->channels[j].dev,  stat.pkt_cnt, 
            (stat.pkt_cnt - stat.capture_packet_cnt));
        oflops_log(now, PCAP_MSG, msg);
        printf("%s\n", msg);
        display_xmit_metrics(j-1, &gen_stat);
        snprintf(msg, 1024, "%s:snd:%u",ctx->channels[j].dev,gen_stat.pkt_snd_cnt);
        oflops_log(now, PCAP_MSG, msg);
        printf("%s\n",msg);
      }
    }
  }

  free(param);
  oflops_log_close();

  fprintf(stderr, "-----------------------------------------------\n");
  fprintf(stderr, "---------------    Finished   -----------------\n");
  fprintf(stderr, "-----------------------------------------------\n");
  return 0;
}
