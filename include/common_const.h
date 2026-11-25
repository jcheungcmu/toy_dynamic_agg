#ifndef CONST_H
#define CONST_H

const uint8_t max_credits = 1;
const uint8_t max_slots = 4;

const uint8_t lock_granted_id = 1;
const uint8_t lock_denied_id = 0;

const uint8_t ack_from_addr_table_id = 1; // ctrl


// packet user IDs
const uint8_t end_of_stream_token_user_id = 0; // data 
const uint8_t flush_token_user_id = 1; // data 
const uint8_t result_token_user_id = 2; // data 
const uint8_t delete_token_user_id = 3; // data

const uint8_t raw_token_user_id = 4; // data 

const uint8_t ack_data_user_id = 5; // ctrl
const uint8_t lock_acquire_user_id = 6; // ctrl
const uint8_t lock_release_user_id = 7; // ctrl
const uint8_t lock_response_user_id = 8; // ctrl
const uint8_t pr_request_user_id = 9; // ctrl
const uint8_t pr_response_user_id = 10; // ctrl


const uint8_t update_prev_from_pr_handler_user_id = 20; // ctrl        for direct tail updates (head node edge case)

const uint8_t send_prev_delete_next_user_id = 14; // ctrl        for deletions
const uint8_t send_prev_insert_next_user_id = 15; // ctrl        for tail updates
const uint8_t send_next_update_prev_user_id = 16; // ctrl        for deletion

// const uint8_t send_prev_release_delete_token_user_id = 21; // ctrl   for deletions with lock release
// const uint8_t release_delete_token_user_id = 22;

const uint8_t delete_next_from_state_mgr_user_id = 11; // ctrl     corresponding to previous 
const uint8_t insert_next_from_state_mgr_user_id = 12; // ctrl
const uint8_t update_prev_from_state_mgr_user_id = 13; // ctrl


const uint8_t state_mgr_to_mgr_response_user_id = 17; // ctrl
const uint8_t state_mgr_to_pr_handler_response_user_id = 18; // ctrl

const uint8_t delete_request_id = 19;

// const uint8_t stop_state_manager_user_id = 20; // ctrl            

// const uint8_t delete_next_from_pr_handler_user_id = 23; // ctrl
// const uint8_t insert_next_from_pr_handler_user_id = 23; // ctrl

// PR module IDs 
// const uint8_t module_id_agg = 0;
const uint8_t stop_pr_kernels_module_id = 255;
const uint8_t pr_request_fail_id = 5;
const uint8_t pr_request_success_id = 0;

const uint8_t pr_handler_to_src_id = 1;
#endif
