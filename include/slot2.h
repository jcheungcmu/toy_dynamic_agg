#ifndef SLOT2_H
#define SLOT2_H

#include <sycl/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>

#include "types.h"
#include "common_pipes.h"
#include "common_const.h"

using namespace sycl;

extern "C" {
  event slot2_agg(queue &q);
  
  void slot2_user_control_handler(queue &q);
  event slot2_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_next_addr);
  void slot2_credits_handler(queue &q, uint8_t src_addr);  
  event slot2_state_manager(queue &q, uint8_t src_addr, uint8_t init_prev_addr, uint8_t init_next_addr);
  event slot2_telemetry(queue &q, uint64_t idle_cycles_threshold, bool enabled);
  event slot2_telemetry_request_gen(queue &q, uint8_t pr_handler_addr, uint8_t src_addr);
  event slot2_pr_handler(queue &q, uint8_t src_addr, uint8_t lock_mgr_addr, uint8_t local_state_mgr_addr);
  event slot2_mux(queue &q);
}


#endif 