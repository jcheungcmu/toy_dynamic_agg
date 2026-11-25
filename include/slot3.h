#ifndef SLOT3_H
#define SLOT3_H

#include <sycl/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>

#include "types.h"
#include "common_pipes.h"
#include "common_const.h"

using namespace sycl;

using results_pipe = ext::intel::pipe<class ResultsPipe, packet_data, 0>;
using overflow_pipe = ext::intel::pipe<class OverflowPipe, packet_data, 0>;

// using stop_results_pipe = ext::intel::pipe<class StopResultsPipe, uint8_t, 0>;
// using stop_overflow_pipe = ext::intel::pipe<class StopOverflowPipe, uint8_t, 0>;
// using stop_fork_pipe = ext::intel::pipe<class StopForkPipe, uint8_t, 0>;

// using overflow_telemetry_pipe = ext::intel::pipe<class OverflowTelemetryPipe, packet_data, 0>;

// using pr_request_filter_to_pr_handler_pipe = ext::intel::pipe<class PRRequestFilterToPRHandlerPipe, pr_request_packet, 0>;

// using pr_handler_to_write_ctrl_pipe = ext::intel::pipe<class PRHandlerToWriteCtrlPipe, packet_ctrl, 0>;
// using read_ctrl_to_pr_handler_pipe = ext::intel::pipe<class ReadCtrlToPRHandlerPipe, packet_ctrl, 0>;

// using pr_handler_to_pr_request_filter_pipe = ext::intel::pipe<class PRHandlerToPRRequestFilterPipe, uint8_t, 0>;



using pr_request_to_host_pipe = ext::intel::pipe<class PipePRRequest, packet_ctrl, 0>;
using pr_response_from_host_pipe = ext::intel::pipe<class PipePRack, packet_ctrl, 0>;

using stop_lock_manager_pipe = ext::intel::pipe<class StopLockManagerPipe, uint8_t, 0>;

using src_to_agg_pipe = ext::intel::pipe<class SrcToAggPipe, packet_data, 0>;
using pr_handler_to_src_pipe = ext::intel::pipe<class PRHandlerToSrcPipe, uint8_t, 0>;

extern "C" {
  event slot3_src(queue &q, buffer<key_value>& input_buf, size_t num_items);
  event slot3_agg(queue &q);
  event slot3_results_handler (queue &q, buffer<key_value>&   result_buf, buffer<uint8_t>& count_buf);
  event slot3_overflow_handler(queue &q, buffer<key_value>& overflow_buf, buffer<size_t>& count_buf);
  event slot3_fork (queue &q);

  void slot3_user_control_handler(queue &q);
  event slot3_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_next_addr);
  void slot3_credits_handler(queue &q, uint8_t src_addr);  
  event slot3_state_manager(queue &q, uint8_t src_addr, uint8_t init_prev_addr, uint8_t init_next_addr);
  event slot3_telemetry(queue &q);
  event slot3_telemetry_request_gen(queue &q, uint8_t pr_handler_addr, uint8_t src_addr);
  event slot3_pr_handler(queue &q, uint8_t src_addr, uint8_t lock_mgr_addr, uint8_t local_state_mgr_addr);
  event slot3_mux(queue &q);


  event pr_request_to_host_kernel(queue &q, buffer<packet_ctrl>& a_buf);
  event pr_response_from_host_kernel(queue &q, packet_ctrl response);
  event lock_manager(queue &q, uint8_t src_addr);


}


#endif 