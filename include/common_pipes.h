#ifndef PIPES_H
#define PIPES_H

#include <sycl/sycl.hpp>
#include <sycl/ext/intel/fpga_extensions.hpp>
#include "types.h"

using namespace sycl;

using write_data_iopipe = ext::intel::kernel_writeable_io_pipe<pipe_id_0, packet_data, 0>;
using read_data_iopipe = ext::intel::kernel_readable_io_pipe<pipe_id_1, packet_data, 0>;

using write_ctrl_iopipe = ext::intel::kernel_writeable_io_pipe<pipe_id_2, packet_ctrl, 0>;
using read_ctrl_iopipe = ext::intel::kernel_readable_io_pipe<pipe_id_3, packet_ctrl, 0>;

// using write_data_shim_pipe = ext::intel::pipe<class ShimPipeWriteData, packet_data, 0>; 
// using read_data_shim_pipe = ext::intel::pipe<class ShimPipeReadData, packet_data, 0>;

// using write_ctrl_shim_pipe = ext::intel::pipe<class ShimPipeWriteCtrl, packet_ctrl, 0>; 
// using read_ctrl_shim_pipe = ext::intel::pipe<class ShimPipeReadCtrl, packet_ctrl, 0>;

using write_data_user_pipe = ext::intel::pipe<class UserPipeWriteData, packet_data, 0>;
using read_data_user_pipe = ext::intel::pipe<class UserPipeReadData, packet_data, 0>;

using write_ctrl_user_pipe = ext::intel::pipe<class UserPipeWriteCtrl, packet_ctrl, 0>;
using read_ctrl_user_pipe = ext::intel::pipe<class UserPipeReadCtrl, packet_ctrl, 0>;

using write_data_with_translated_addr_pipe = ext::intel::pipe<class UserPipeWriteDataWithTranslatedAddr, packet_data, 0>;
// using read_data_addr_translation_pipe = ext::intel::pipe<class UserPipeReadDataAddrTranslation, packet_data, 0>;

using credit_pipe = ext::intel::pipe<class CreditPipe, uint8_t, 0>;
using update_dest_pipe = ext::intel::pipe<class UpdateDestPipe, packet_ctrl, 0>;


// stops credit handler
using stop_read_data_pipe = ext::intel::pipe<class StopReadDataPipe, uint8_t, 0>;
using stop_write_ctrl_pipe = ext::intel::pipe<class StopWriteCtrlPipe, uint8_t, 0>;
using stop_read_ctrl_pipe = ext::intel::pipe<class StopReadCtrlPipe, uint8_t, 0>;

// stops user control handler
using stop_write_ctrl_user_pipe = ext::intel::pipe<class StopWriteCtrlUserPipe, uint8_t, 0>;
using stop_read_ctrl_user_pipe = ext::intel::pipe<class StopReadCtrlUserPipe, uint8_t, 0>;

using write_ack_data_pipe = ext::intel::pipe<class WriteAckDataPipe, packet_ctrl, 0>;
// using write_ack_tail_update_pipe = ext::intel::pipe<class WriteAckTailUpdatePipe, packet_ctrl, 0>;


using write_pr_request_pipe = ext::intel::pipe<class PipeWritePRRequest, packet_ctrl, 0>;
using read_pr_request_pipe = ext::intel::pipe<class PipeReadPRRequest, packet_ctrl, 0>;

using write_pr_response_pipe = ext::intel::pipe<class PipeWritePRResponse, packet_ctrl, 0>;
using read_pr_response_pipe = ext::intel::pipe<class PipeReadPRResponse, packet_ctrl, 0>;

using write_lock_acquire_pipe = ext::intel::pipe<class PipeWriteLockAcquire, packet_ctrl, 0>;
using read_lock_acquire_pipe = ext::intel::pipe<class PipeReadLockAcquire, packet_ctrl, 0>;

using write_lock_release_pipe = ext::intel::pipe<class PipeWriteLockRelease, packet_ctrl, 0>;
using read_lock_release_pipe = ext::intel::pipe<class PipeReadLockRelease, packet_ctrl, 0>;

using write_lock_response_pipe = ext::intel::pipe<class PipeWriteLockResponse, packet_ctrl, 0>;
using read_lock_response_pipe = ext::intel::pipe<class PipeReadLockResponse, packet_ctrl, 0>;

// using write_update_ptr_pipe = ext::intel::pipe<class PipeWriteUpdateNext, packet_ctrl, 0>;
// using read_update_ptr_pipe = ext::intel::pipe<class PipeReadUpdateNext, packet_ctrl, 0>;

// using write_ack_update_ptr_pipe = ext::intel::pipe<class PipeWriteAckUpdateNext, packet_ctrl, 0>;
// using read_ack_update_ptr_pipe = ext::intel::pipe<class PipeReadAckUpdateNext, packet_ctrl, 0>;

using telemetry_to_request_pipe = ext::intel::pipe<class TelemetryToRequestPipe, uint8_t, 0>;
using read_data_telemetry_pipe = ext::intel::pipe<class ReadDataTelemetryPipe, packet_data, 0>;

// using write_update_next_pipe = ext::intel::pipe<class PipeWriteUpdateNext, packet_ctrl, 0>;
// using read_update_next_pipe = ext::intel::pipe<class PipeReadUpdateNext, packet_ctrl, 0>;

// using write_ack_update_next_pipe = ext::intel::pipe<class PipeWriteAckUpdateNext, packet_ctrl, 0>;
// using read_ack_update_next_pipe = ext::intel::pipe<class PipeReadAckUpdateNext, packet_ctrl, 0>;

// using write_update_prev_pipe = ext::intel::pipe<class PipeWriteUpdatePrev, packet_ctrl, 0>;
// using read_update_prev_pipe = ext::intel::pipe<class PipeReadUpdatePrev, packet_ctrl, 0>;

// using write_ack_update_prev_pipe = ext::intel::pipe<class PipeWriteAckUpdatePrev, packet_ctrl, 0>;
// using read_ack_update_prev_pipe = ext::intel::pipe<class PipeReadAckUpdatePrev, packet_ctrl, 0>;

using update_addr_table_pipe = ext::intel::pipe<class UpdateAddrTablePipe, packet_ctrl, 0>;
using ack_update_addr_table_pipe = ext::intel::pipe<class AckUpdateAddrTablePipe, uint8_t, 0>;

using write_state_manager_request_pipe = ext::intel::pipe<class WriteStateManagerRequestPipe, packet_ctrl, 0>;
using write_state_manager_response_pipe = ext::intel::pipe<class WriteStateManagerResponsePipe, packet_ctrl, 0>;

using read_state_manager_request_pipe = ext::intel::pipe<class ReadStateManagerRequestPipe, packet_ctrl, 0>;
using read_state_manager_response_pipe = ext::intel::pipe<class ReadStateManagerResponsePipe, packet_ctrl, 0>;


using write_pr_handler_to_state_mgr_request_pipe = ext::intel::pipe<class WritePRHandlerToStateManagerRequestPipe, packet_ctrl, 0>;
// using write_state_manager_to_pr_handler_response_pipe = ext::intel::pipe<class WriteStateManagerToPRHandlerResponsePipe, packet_ctrl, 0>;

// using read_pr_handler_to_state_manager_request_pipe = ext::intel::pipe<class ReadPRHandlerToStateManagerRequestPipe, packet_ctrl, 0>;
using read_state_mgr_to_pr_handler_response_pipe = ext::intel::pipe<class ReadStateManagerToPRHandlerResponsePipe, packet_ctrl, 0>;


using mux0_pipe = ext::intel::pipe<class Mux0Pipe, packet_data, 0>;
using mux1_pipe = ext::intel::pipe<class Mux1Pipe, packet_data, 0>;


using write_pr_handler_pipe = ext::intel::pipe<class WritePRHandlerPipe, packet_ctrl, 0>;

using stop_state_manager_pipe = ext::intel::pipe<class StopStateManagerPipe, uint8_t, 0>;
// extern "C" {
//   event iopipe_shim(queue &q);
//   event credits_handler(queue &q, uint8_t src_addr);
// }

#endif