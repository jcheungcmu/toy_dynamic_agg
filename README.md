# cascade_agg

## Using the Control Pipe

In `cascade_agg`, control messages are routed across FPGA slots over the NoC using a two-layer control pipe architecture that separates hardware I/O & credit handling from user/PR kernel logic.

### 1. Control Packet Format (`include/types.h`)

All control pipes carry `packet_ctrl`:

```cpp
struct packet_ctrl {
  uint8_t dest_addr; // Must be the first field for NoC routing extraction
  uint8_t src_addr;  // Source slot address (used for replies/ACKs)
  uint8_t user;      // Message type ID (defined in include/common_const.h)
  uint8_t payload;   // 8-bit control payload
};
```

### 2. Pipe Architecture (`include/common_pipes.h`)

User kernels **must not** access the hardware I/O pipes (`write_ctrl_iopipe` / `read_ctrl_iopipe`) directly. Instead, control traffic flows through two layers:

1. **Credits Handler (`slotX_credits_handler`)** — owns the NoC I/O pipes (`pipe_id_2` / `pipe_id_3`):
   - **Outbound (`write_ctrl_credits_handler`)**: Arbitrates between `write_ctrl_user_pipe` (high priority) and `write_ack_data_pipe` (low priority, data-plane credit ACKs) and writes to `write_ctrl_iopipe`.
   - **Inbound (`read_ctrl_credits_handler`)**: Reads from `read_ctrl_iopipe`. Packets with `user == ack_data_user_id` (`5`) are consumed as credits via `credit_pipe`; all other packets are forwarded to `read_ctrl_user_pipe`.
2. **User Control Handler (`slotX_user_control_handler`)** — multiplexes/demultiplexes `write_ctrl_user_pipe` and `read_ctrl_user_pipe` across internal kernel pipes:
   - **Outbound (`write_ctrl_handler`)**: Non-blocking-polls internal request/response pipes (e.g., `write_pr_request_pipe`, `write_state_manager_request_pipe`, `write_state_manager_response_pipe`, `write_pr_handler_pipe`) and forwards valid packets to `write_ctrl_user_pipe`.
   - **Inbound (`read_ctrl_handler`)**: Reads from `read_ctrl_user_pipe` and routes packets by `packet.user` to the matching internal pipe (e.g., `read_pr_request_pipe`, `read_pr_response_pipe`, `read_lock_response_pipe`, `read_state_manager_request_pipe`, `read_state_manager_response_pipe`, `read_state_mgr_to_pr_handler_response_pipe`).

### 3. Adding a New Control Message

1. **Define a `user` ID** in `include/common_const.h` that does not collide with existing IDs (e.g., `0`–`20` are currently assigned, including `ack_data_user_id = 5`).
2. **Declare internal pipe(s)** of type `packet_ctrl` in `include/common_pipes.h`:
   ```cpp
   using my_write_ctrl_pipe = ext::intel::pipe<class MyWriteCtrlPipe, packet_ctrl, 0>;
   using my_read_ctrl_pipe  = ext::intel::pipe<class MyReadCtrlPipe, packet_ctrl, 0>;
   ```
3. **Send a control packet** from your kernel by populating `packet_ctrl` and writing to your internal pipe:
   ```cpp
   packet_ctrl pkt;
   pkt.dest_addr = target_slot_addr;
   pkt.src_addr  = my_slot_addr;
   pkt.user      = my_custom_user_id;
   pkt.payload   = my_payload;
   my_write_ctrl_pipe::write(pkt);
   ```
4. **Wire it into `slotX_user_control_handler`** (in `slot*/helper.cpp` or `slot3/src.cpp`):
   - In `write_ctrl_handler`, poll `my_write_ctrl_pipe::read(valid)` and forward to `write_ctrl_user_pipe::write(...)`.
   - In `read_ctrl_handler`, match `read_ctrl_packet.user == my_custom_user_id` and forward to `my_read_ctrl_pipe::write(read_ctrl_packet)`.

### 4. Shutdown

When `write_data_credits_handler` processes a `flush_token_user_id` or a forwarded `delete_token_user_id`, it automatically signals `stop_read_ctrl_pipe`, `stop_write_ctrl_pipe`, `stop_read_ctrl_user_pipe`, and `stop_write_ctrl_user_pipe` so the control handlers exit cleanly.
