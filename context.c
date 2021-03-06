#include <string.h>
#include <dlfcn.h>

#include "context.h"
#include "timer_event.h"
#include "utils.h"
#include "log.h"

#include <pcap.h>

#include <openflow/openflow.h>


/**
 * an oflops context generation and initialization method
 * \return a pointer to the new oflops context details
 */
oflops_context * oflops_default_context(void) {

  //initialize oflops nf packet generator (enable packet padding)
//  nf_init(1, 0, 0);

  oflops_context * ctx = malloc_and_check(sizeof(oflops_context));
  bzero(ctx, sizeof(*ctx));
  ctx->max_tests = 10 ;
  ctx->tests = malloc_and_check(ctx->max_tests * sizeof(test_module *));

  ctx->listen_port = OFP_TCP_PORT;	// listen on default port

  ctx->snaplen = 112;

  ctx->n_channels=1;
  ctx->max_channels=10;
  ctx->channels = malloc_and_check(sizeof(struct channel_info)* ctx->max_channels);

  ctx->snmp_channel_info = malloc_and_check(sizeof(struct snmp_channel));
  memset(ctx->snmp_channel_info, 0 , sizeof(struct snmp_channel));
  ctx->channels[OFLOPS_CONTROL].raw_sock = -1;

  // initalize other channels later
  ctx->log = malloc(sizeof(DEFAULT_LOG_FILE));
  strcpy(ctx->log, DEFAULT_LOG_FILE);

  ctx->trafficGen = PKTGEN;

  ctx->dump_controller = 0;
  ctx->cpuOID_count = 0;
  ctx->started = 0;
  ctx->fluid_control = NULL;
  return ctx;
}

/**
  * a method to reinit an oflops context structure.
  * to be run me between tests.
  * \param ctx a pointer to the context object
  */
int reset_context(oflops_context * ctx) {
  // close the open lirary object
  if(ctx->curr_test)
    dlclose(ctx->curr_test->symbol_handle);
  return 0;
}
