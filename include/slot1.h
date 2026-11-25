#ifndef SLOT1_H
#define SLOT1_H

#include <sycl/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>

#include "types.h"
#include "common_pipes.h"
#include "common_const.h"

using namespace sycl;

extern "C" {
  event slot1_agg(queue &q);
  
  void slot1_user_control_handler(queue &q);
  event slot1_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_next_addr);
  void slot1_credits_handler(queue &q, uint8_t src_addr);  
  event slot1_state_manager(queue &q, uint8_t src_addr, uint8_t init_prev_addr, uint8_t init_next_addr);
  event slot1_telemetry(queue &q, uint64_t idle_cycles_threshold, bool enabled);
  event slot1_telemetry_request_gen(queue &q, uint8_t pr_handler_addr, uint8_t src_addr);
  event slot1_pr_handler(queue &q, uint8_t src_addr, uint8_t lock_mgr_addr, uint8_t local_state_mgr_addr);
  event slot1_mux(queue &q);
}


#endif 