#include "slot2.h"

extern "C" {
  event slot2_agg(queue &q) {

    return q.submit([&](handler &h) {

      h.single_task<class agg_task> ([=]() { 

        packet_data write_data_packet;
        packet_data read_data_packet;

        write_data_packet.dest_addr = 0; // will be overwritten
        write_data_packet.src_addr = 0; // will be overwritten

        key_value local_table;

        uint8_t isFirstKey = 1;

        while(1) {

          read_data_packet = read_data_user_pipe::read();

          if (read_data_packet.user == raw_token_user_id) {

            if (isFirstKey == 1) {
              local_table.key = read_data_packet.payload.key;
              local_table.value = read_data_packet.payload.value;
              isFirstKey = 0;
            } 
            else { 
              if (read_data_packet.payload.key == local_table.key) {
                local_table.value += read_data_packet.payload.value;
              }
              else { // forward the key
                write_data_packet.user = read_data_packet.user; 
                write_data_packet.payload = read_data_packet.payload;
                write_data_user_pipe::write(write_data_packet);
              }
            } 


          }
          else if (read_data_packet.user == result_token_user_id || read_data_packet.user == end_of_stream_token_user_id) { 
            // forward the result or end of stream token
            write_data_packet.user = read_data_packet.user; 
            write_data_packet.payload = read_data_packet.payload;
            write_data_user_pipe::write(write_data_packet);
          }

          else if (read_data_packet.user == flush_token_user_id || 
                   read_data_packet.user == delete_token_user_id) {

            if (isFirstKey == 0) {
              // there is an aggregated result to send
              write_data_packet.user = result_token_user_id; // tag as result 
              write_data_packet.payload = local_table;
              write_data_user_pipe::write(write_data_packet);
            }

            // forward the flush token 
            write_data_packet.user = read_data_packet.user;
            write_data_packet.payload = read_data_packet.payload; // payload is don't care
            write_data_user_pipe::write(write_data_packet); // forward the flush token
            break;
          }
        }
        
      });

    });
  }

  event slot2_mux(queue &q) {
    return q.submit([&](handler &h) {

      h.single_task<class mux_task> ([=]() { 

        packet_data read_data_packet;
        bool read_data_valid = false;

        packet_data read_delete_packet;
        bool read_delete_valid = false;

        while(1) {

          read_delete_packet = mux0_pipe::read(read_delete_valid);
          if (read_delete_valid) {
            // send delete token
            write_data_with_translated_addr_pipe::write(read_delete_packet);
          }
          else {
            // normal data
            read_data_packet = mux1_pipe::read(read_data_valid);
            if (read_data_valid) {
              write_data_with_translated_addr_pipe::write(read_data_packet);

              if (read_data_packet.user == flush_token_user_id || 
                  read_data_packet.user == delete_token_user_id) {
                break;
              }
            }
          }

        }

        
      });

    });
  }

  event slot2_telemetry(queue &q, uint64_t idle_cycles_threshold, bool enabled) {
    return q.submit([&](handler &h) {

      h.single_task<class telemetry_task> ([=]() { 

        packet_data read_data_packet;
        bool read_data_valid = false;

        uint64_t count = 0;

        bool stopped_pr = false;

        while(1) { 
          read_data_packet = read_data_telemetry_pipe::read(read_data_valid);

          if (read_data_valid) {
            count = 0;
            
            // close request generator and pr handler if end of stream token is seen
            // don't break because flush token will need to be forwarded
            if (read_data_packet.user == end_of_stream_token_user_id) { 

              if (stopped_pr == false) {
                telemetry_to_request_pipe::write(stop_pr_kernels_module_id);
                stopped_pr = true;
              }
              read_data_user_pipe::write(read_data_packet);
            }
            // closes when it sees flush or delete
            else if (read_data_packet.user == flush_token_user_id || read_data_packet.user == delete_token_user_id) { 
              if (stopped_pr == false) {
                telemetry_to_request_pipe::write(stop_pr_kernels_module_id);
                stopped_pr = true;
              }
              read_data_user_pipe::write(read_data_packet);
              break;    
            }
            else {
              read_data_user_pipe::write(read_data_packet);
            }

          }
          else {
            count++;
            
            if (count > idle_cycles_threshold) { 
              if (enabled == true) {
                telemetry_to_request_pipe::write(delete_request_id); // don't know if delete was successful or not
              }
            }
          }

        }
      });

    });
  }


  event slot2_telemetry_request_gen(queue &q, uint8_t pr_handler_addr, uint8_t src_addr) { 

    return q.submit([&](handler &h) {

      h.single_task<class telemetry_request_gen> ([=]() { 

        uint8_t telemetry_request;
        bool telemetry_request_valid = false;

        packet_ctrl pr_response;
        bool pr_response_valid = false;

        bool request_in_progress = false;        

        packet_ctrl write_ctrl_packet;

        bool stopped = false;

        while(1) { 
          
          pr_response = read_pr_response_pipe::read(pr_response_valid);

          if (pr_response_valid) {

            // if PR handler responds with success, deletion will happen
            // no more requests can be generated
            if (pr_response.payload == pr_request_success_id) {
              stopped = true;
            }
            request_in_progress = false;

          }

          else {

            telemetry_request = telemetry_to_request_pipe::read(telemetry_request_valid);

            // consume requests
            // multiple delete requests can be generated because we don't know if the first delete was successful or not
            if (telemetry_request_valid) {

              // stop signal from telemetry means it saw end of stream or delete token
              // only one stop signal is generated, always write send it even if a request is in progress 
              // pr handler only stops from stop token
              if (telemetry_request == stop_pr_kernels_module_id) {

                write_ctrl_packet.dest_addr = pr_handler_addr; // dest is previous tail addr
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.user = pr_request_user_id; 
                write_ctrl_packet.payload = telemetry_request;
                write_pr_request_pipe::write(write_ctrl_packet);
                break;

              }
              else {

                if (request_in_progress == false && stopped == false) {

                  // send PR request
                  write_ctrl_packet.dest_addr = pr_handler_addr; // dest is previous tail addr
                  write_ctrl_packet.src_addr = src_addr;
                  write_ctrl_packet.user = pr_request_user_id; 
                  write_ctrl_packet.payload = telemetry_request; 
                  write_pr_request_pipe::write(write_ctrl_packet);
                  request_in_progress = true;
                }

              }
            }

          }
        }
      });

    });
  }


  event slot2_pr_handler(queue &q, uint8_t src_addr, uint8_t lock_mgr_addr, uint8_t local_state_mgr_addr) { 
    return q.submit([&](handler &h) {

      h.single_task<class pr_handler_task> ([=]() { 

        packet_ctrl request;

        packet_ctrl write_ctrl_packet;
        packet_ctrl read_ctrl_packet;

        packet_data write_data_packet;
        bool write_success = false;

        while(1) { 

          request = read_pr_request_pipe::read(); 

          if (request.payload == stop_pr_kernels_module_id) {
            break;
          }
          else if (request.payload == delete_request_id) { // valid delete request


            // try to acquire lock 
            write_ctrl_packet.dest_addr = lock_mgr_addr; // dest is lock manager addr
            write_ctrl_packet.src_addr = src_addr;
            write_ctrl_packet.user = lock_acquire_user_id; // reserved for update dest
            write_ctrl_packet.payload = request.payload; // don't care 

            // write_lock_acquire_pipe::write(write_ctrl_packet); // send acquire lock packet
            write_pr_handler_pipe::write(write_ctrl_packet);

            // wait for lock response
            read_ctrl_packet = read_lock_response_pipe::read();

            if (read_ctrl_packet.payload == lock_denied_id) { // lock denied
              // send nack to pr_request_filter to try again later
              write_ctrl_packet.dest_addr = request.src_addr; // dest is lock manager addr
              write_ctrl_packet.src_addr = src_addr;
              write_ctrl_packet.user = pr_response_user_id; // reserved for update dest
              write_ctrl_packet.payload = pr_request_fail_id; // don't care 

              // write_pr_response_pipe::write(write_ctrl_packet); // send nack packet
              write_pr_handler_pipe::write(write_ctrl_packet);
            }
            else if (read_ctrl_packet.payload == lock_granted_id) {

              // lock granted

              // update next's prev with prev
              write_ctrl_packet.dest_addr = local_state_mgr_addr; // dest is previous tail addr
              write_ctrl_packet.src_addr = src_addr;
              write_ctrl_packet.user = send_next_update_prev_user_id; // update old tail's next pointer
              write_ctrl_packet.payload = 0; // don't care 

              // write_pr_handler_to_state_mgr_request_pipe::write(write_ctrl_packet);
              write_pr_handler_pipe::write(write_ctrl_packet);
              
              read_ctrl_packet = read_state_mgr_to_pr_handler_response_pipe::read();
              
              if (read_ctrl_packet.user == state_mgr_to_pr_handler_response_user_id) { 

                // update prev's next with next
                write_ctrl_packet.dest_addr = local_state_mgr_addr; // dest is previous tail addr
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.user = send_prev_delete_next_user_id; // update tail ptr to new tail
                write_ctrl_packet.payload = 0; // don't care

                // write_pr_handler_to_state_mgr_request_pipe::write(write_ctrl_packet);
                write_pr_handler_pipe::write(write_ctrl_packet);

                read_ctrl_packet = read_state_mgr_to_pr_handler_response_pipe::read();
          
                // flush token is on the way 

                if (read_ctrl_packet.user == state_mgr_to_pr_handler_response_user_id) { // ack should always come from previous tail

                  // release the lock 
                  write_ctrl_packet.dest_addr = lock_mgr_addr; // dest is previous tail addr
                  write_ctrl_packet.src_addr = src_addr;
                  write_ctrl_packet.user = lock_release_user_id; // reserved for update dest
                  write_ctrl_packet.payload = 0; // don't care

                  // write_lock_release_pipe::write(write_ctrl_packet); // send update dest packet
                  write_pr_handler_pipe::write(write_ctrl_packet);
                  
                  // race between ack and delete token arrival
                  // send ack to pr_request_filter 
                  write_ctrl_packet.dest_addr = request.src_addr; // dest is lock manager addr
                  write_ctrl_packet.src_addr = src_addr;
                  write_ctrl_packet.user = pr_response_user_id; // reserved for update dest
                  write_ctrl_packet.payload = pr_request_success_id; // don't care 
                
                  // write_pr_response_pipe::write(write_ctrl_packet); // send ack packet
                  write_pr_handler_pipe::write(write_ctrl_packet);

                }
              }



            }


          }
  
        }

      });

    });
  }
  

  event slot2_state_manager(queue &q, uint8_t src_addr, uint8_t init_prev_addr, uint8_t init_next_addr) { 
    return q.submit([&](handler &h) {

      h.single_task<class state_manager_task> ([=]() { 

        uint8_t prev = init_prev_addr;
        uint8_t next = init_next_addr;

        packet_ctrl request;
        bool request_valid = false;

        packet_ctrl write_ctrl_packet;

        packet_ctrl read_ctrl_packet;

        packet_ctrl ack_stall;

        uint8_t ack_update_addr_table;

        packet_data write_data_packet;
  
        uint8_t read_stop;
        bool stop_valid = false;

        while(1) {

          read_stop = stop_state_manager_pipe::read(stop_valid);

          if (stop_valid) {
            break;
          }

          else { 

            request = read_state_manager_request_pipe::read(request_valid); 

            if (request_valid) {

              if (request.user == update_prev_from_pr_handler_user_id) { // head edge case update tail
                prev = request.payload;

                write_ctrl_packet.dest_addr = request.src_addr;
                write_ctrl_packet.user = state_mgr_to_pr_handler_response_user_id;
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.payload = 0; // don't care
                write_state_manager_response_pipe::write(write_ctrl_packet); // ack to state manager

              }

              else if (request.user == insert_next_from_state_mgr_user_id) { 

                update_addr_table_pipe::write(request); // forward to addr table
                ack_update_addr_table = ack_update_addr_table_pipe::read(); // wait for ack
                
                if (ack_update_addr_table == ack_from_addr_table_id) {
                  next = request.payload;

                  write_ctrl_packet.dest_addr = request.src_addr;
                  write_ctrl_packet.user = state_mgr_to_mgr_response_user_id;;
                  write_ctrl_packet.src_addr = src_addr;
                  write_ctrl_packet.payload = 0; // don't care

                  write_state_manager_response_pipe::write(write_ctrl_packet); // ack to state manager

                }
                
              }

              else if (request.user == delete_next_from_state_mgr_user_id) { 

                update_addr_table_pipe::write(request); // forward to addr table
                ack_update_addr_table = ack_update_addr_table_pipe::read(); // wait for ack
                
                if (ack_update_addr_table == ack_from_addr_table_id) {

                  // insert delete token into datapath
                  write_data_packet.user = delete_token_user_id;
                  write_data_packet.payload = {0, 0}; // don't care
                  write_data_packet.src_addr = src_addr;
                  write_data_packet.dest_addr = next; // old next
                  mux0_pipe::write(write_data_packet);

                  next = request.payload;


                  write_ctrl_packet.dest_addr = request.src_addr;
                  write_ctrl_packet.user = state_mgr_to_mgr_response_user_id;;
                  write_ctrl_packet.src_addr = src_addr;
                  write_ctrl_packet.payload = 0; // don't care

                  write_state_manager_response_pipe::write(write_ctrl_packet); // ack to state manager

                }
                
              }

              else if (request.user == update_prev_from_state_mgr_user_id) { 
                prev = request.payload;

                write_ctrl_packet.dest_addr = request.src_addr;
                write_ctrl_packet.user = state_mgr_to_mgr_response_user_id;
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.payload = 0; // don't care

                write_state_manager_response_pipe::write(write_ctrl_packet); // ack to state manager

              }

              else if (request.user == send_prev_insert_next_user_id) { 

                if (prev == src_addr) { // edge case, update own next
                  update_addr_table_pipe::write(request); // forward to addr table
                  ack_update_addr_table = ack_update_addr_table_pipe::read(); // wait for ack
                  
                  if (ack_update_addr_table == ack_from_addr_table_id) {
                    next = request.payload;

                    write_ctrl_packet.dest_addr = request.src_addr;
                    write_ctrl_packet.user = state_mgr_to_pr_handler_response_user_id;
                    write_ctrl_packet.src_addr = src_addr;
                    write_ctrl_packet.payload = 0; // don't care

                    write_state_manager_response_pipe::write(write_ctrl_packet); // ack to pr handler
                  }
                }
                else { 
                  write_ctrl_packet.dest_addr = prev;
                  write_ctrl_packet.user = insert_next_from_state_mgr_user_id;
                  write_ctrl_packet.src_addr = src_addr;
                  write_ctrl_packet.payload = request.payload; // new next

                  write_state_manager_request_pipe::write(write_ctrl_packet);

                  read_ctrl_packet = read_state_manager_response_pipe::read();

                  if (read_ctrl_packet.user == state_mgr_to_mgr_response_user_id) {
                    write_ctrl_packet.dest_addr = request.src_addr;
                    write_ctrl_packet.user = state_mgr_to_pr_handler_response_user_id;
                    write_ctrl_packet.src_addr = src_addr;
                    write_ctrl_packet.payload = 0; // don't care

                    write_state_manager_response_pipe::write(write_ctrl_packet); // ack to pr handler

                  }
                }

              } 


              // for deleting
              else if (request.user == send_prev_delete_next_user_id) { // only when 2 nodes exist, no edge case

                write_ctrl_packet.dest_addr = prev;
                write_ctrl_packet.user = delete_next_from_state_mgr_user_id;
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.payload = next; // new next

                write_state_manager_request_pipe::write(write_ctrl_packet);

                read_ctrl_packet = read_state_manager_response_pipe::read();

                if (read_ctrl_packet.user == state_mgr_to_mgr_response_user_id) {
                  write_ctrl_packet.dest_addr = request.src_addr;
                  write_ctrl_packet.user = state_mgr_to_pr_handler_response_user_id;
                  write_ctrl_packet.src_addr = src_addr;
                  write_ctrl_packet.payload = 0; // don't care

                  write_state_manager_response_pipe::write(write_ctrl_packet); // ack to pr handler

                }

              }

              else if (request.user == send_next_update_prev_user_id) {  // only when 2 nodes exist, no edge case

                write_ctrl_packet.dest_addr = next;
                write_ctrl_packet.user = update_prev_from_state_mgr_user_id;
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.payload = prev; // new prev

                write_state_manager_request_pipe::write(write_ctrl_packet);

                read_ctrl_packet = read_state_manager_response_pipe::read();

                if (read_ctrl_packet.user == state_mgr_to_mgr_response_user_id) {
                  write_ctrl_packet.dest_addr = request.src_addr;
                  write_ctrl_packet.user = state_mgr_to_pr_handler_response_user_id;
                  write_ctrl_packet.src_addr = src_addr;
                  write_ctrl_packet.payload = 0; // don't care

                  write_state_manager_response_pipe::write(write_ctrl_packet); // ack to pr handler

                }

              }


            }
          }
        }

      });

    });
  }
  
  event slot2_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_next_addr) {

    return q.submit([&](handler &h) {

      h.single_task<class write_data_translation_task> ([=]() { 

        packet_data write_data_packet;
        packet_data read_data_packet;

        bool read_data_valid = false;
        uint8_t next_addr = init_next_addr; // always initially set to head addr
        uint8_t src_addr = init_src_addr;

        packet_ctrl update_next_packet;
        bool update_next_valid = false; 

        while(1) {

          read_data_packet = write_data_user_pipe::read(read_data_valid);
          update_next_packet = update_addr_table_pipe::read(update_next_valid);

          // race on next_addr doesn't matter
          if (update_next_valid) { 
            next_addr = update_next_packet.payload;

            ack_update_addr_table_pipe::write(ack_from_addr_table_id); // ack to state manager
          }

          if (read_data_valid) {
            write_data_packet.user = read_data_packet.user;
            write_data_packet.payload = read_data_packet.payload;
            write_data_packet.src_addr = src_addr;
            write_data_packet.dest_addr = next_addr;
            
            // write_data_with_translated_addr_pipe::write(write_data_packet);
            mux1_pipe::write(write_data_packet);

            if (write_data_packet.user == flush_token_user_id ||
                write_data_packet.user == delete_token_user_id) {
              break;
            }
          }
          
        }

        
      });

    });
  }



  void slot2_user_control_handler(queue &q) {
    q.submit([&](handler &h) {

      h.single_task<class write_ctrl_handler> ([=]() { 

        // packet_ctrl pr_response_packet;
        // bool pr_response_packet_valid = false;

        packet_ctrl pr_request_packet;
        bool pr_request_packet_valid = false;

        // packet_ctrl lock_acquire_packet;
        // bool lock_acquire_packet_valid = false;

        // packet_ctrl lock_release_packet;
        // bool lock_release_packet_valid = false;

        // packet_ctrl lock_response_packet;
        // bool lock_response_packet_valid = false;

        // packet_ctrl update_next_packet;
        // bool update_next_packet_valid = false;

        // packet_ctrl ack_update_next_packet;
        // bool ack_update_next_packet_valid = false;

        packet_ctrl state_manager_request_packet;
        bool state_manager_request_packet_valid = false;

        packet_ctrl state_manager_response_packet;
        bool state_manager_response_packet_valid = false;

        // packet_ctrl pr_handler_to_state_manager_request_packet; 
        // bool pr_handler_to_state_manager_request_packet_valid = false;

        packet_ctrl state_mgr_to_pr_handler_response_packet;
        bool state_mgr_to_pr_handler_response_packet_valid = false;

        packet_ctrl pr_handler_packet;
        bool pr_handler_packet_valid = false;

        packet_ctrl write_ctrl_packet;

        uint8_t read_stop;
        bool stop_valid = false;

        while(1) {

          read_stop = stop_write_ctrl_user_pipe::read(stop_valid);
          
          if (stop_valid) { 
            break;
          }

          pr_request_packet = write_pr_request_pipe::read(pr_request_packet_valid);

          if (pr_request_packet_valid == true) {

            write_ctrl_packet = pr_request_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);

          }


          // pr_response_packet = write_pr_response_pipe::read(pr_response_packet_valid);

          // if (pr_response_packet_valid == true) {

          //   write_ctrl_packet = pr_response_packet;
          //   write_ctrl_user_pipe::write(write_ctrl_packet);

          // }

          // lock_acquire_packet = write_lock_acquire_pipe::read(lock_acquire_packet_valid);

          // if (lock_acquire_packet_valid == true) {

          //   write_ctrl_packet = lock_acquire_packet;
          //   write_ctrl_user_pipe::write(write_ctrl_packet);
          // }

          // lock_release_packet = write_lock_release_pipe::read(lock_release_packet_valid);

          // if (lock_release_packet_valid == true) {

          //   write_ctrl_packet = lock_release_packet;
          //   write_ctrl_user_pipe::write(write_ctrl_packet);
          // }

          // lock_response_packet = write_lock_response_pipe::read(lock_response_packet_valid);

          // if (lock_response_packet_valid == true) {

          //   write_ctrl_packet = lock_response_packet;
          //   write_ctrl_user_pipe::write(write_ctrl_packet);
          // }

          state_manager_request_packet = write_state_manager_request_pipe::read(state_manager_request_packet_valid);

          if (state_manager_request_packet_valid == true) {

            write_ctrl_packet = state_manager_request_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);
          }

          state_manager_response_packet = write_state_manager_response_pipe::read(state_manager_response_packet_valid);
          if (state_manager_response_packet_valid == true) {

            write_ctrl_packet = state_manager_response_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);

          }

          // pr_handler_to_state_manager_request_packet = write_pr_handler_to_state_mgr_request_pipe::read(pr_handler_to_state_manager_request_packet_valid);
          // if (pr_handler_to_state_manager_request_packet_valid == true) {

          //   write_ctrl_packet = pr_handler_to_state_manager_request_packet;
          //   write_ctrl_user_pipe::write(write_ctrl_packet);

          // }

          pr_handler_packet = write_pr_handler_pipe::read(pr_handler_packet_valid);
          if (pr_handler_packet_valid == true) {
            write_ctrl_packet = pr_handler_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);
          }




        }

      });
    });

    q.submit([&](handler &h) {

      h.single_task<class read_ctrl_handler> ([=]() { 
        packet_ctrl read_ctrl_packet;
        bool read_ctrl_valid = false;

        uint8_t read_stop;
        bool stop_valid = false;

        while(1) {

          read_stop = stop_read_ctrl_user_pipe::read(stop_valid);

          if (stop_valid) { 
            break;
          }    

          read_ctrl_packet = read_ctrl_user_pipe::read(read_ctrl_valid);

          if (read_ctrl_valid) {

            if (read_ctrl_packet.user == pr_request_user_id) {
              read_pr_request_pipe::write(read_ctrl_packet); // forward to pr request
            }
            else if (read_ctrl_packet.user == pr_response_user_id) {
              read_pr_response_pipe::write(read_ctrl_packet); // forward to pr response
            }
            // else if (read_ctrl_packet.user == lock_acquire_user_id) {
            //   read_lock_acquire_pipe::write(read_ctrl_packet); // forward to lock acquire
            // }
            // else if (read_ctrl_packet.user == lock_release_user_id) {
            //   read_lock_release_pipe::write(read_ctrl_packet); // forward to lock release
            // }
            else if (read_ctrl_packet.user == lock_response_user_id) {
              read_lock_response_pipe::write(read_ctrl_packet); // forward to lock response
            }
            else if (

                read_ctrl_packet.user == send_prev_insert_next_user_id ||   
                read_ctrl_packet.user == send_prev_delete_next_user_id ||
                read_ctrl_packet.user == send_next_update_prev_user_id ||

                read_ctrl_packet.user == insert_next_from_state_mgr_user_id ||
                read_ctrl_packet.user == delete_next_from_state_mgr_user_id ||
                read_ctrl_packet.user == update_prev_from_state_mgr_user_id ||

                read_ctrl_packet.user == update_prev_from_pr_handler_user_id 
              
              ) {
              read_state_manager_request_pipe::write(read_ctrl_packet); // forward to state manager
            }
            else if (read_ctrl_packet.user == state_mgr_to_mgr_response_user_id) {
              read_state_manager_response_pipe::write(read_ctrl_packet); // forward to state manager
            }
            else if (read_ctrl_packet.user == state_mgr_to_pr_handler_response_user_id) {
              read_state_mgr_to_pr_handler_response_pipe::write(read_ctrl_packet); // forward to pr handler
            }
          }

        }
      });
    });

  }

  void slot2_credits_handler(queue &q, uint8_t src_addr) {

    q.submit([&](handler &h) {

      h.single_task<class write_data_credits_handler> ([=]() { 

        packet_data write_data_packet;
        uint8_t total_credits = max_credits; // init to max credits 
        
        uint8_t new_credit;

        bool read_data_valid = false;

        bool credit_valid = false;
        while(1) {

          if (total_credits > 0) {

            write_data_packet = write_data_with_translated_addr_pipe::read(read_data_valid);

            if (read_data_valid) {

              bool delete_cond = (write_data_packet.user == delete_token_user_id) && (write_data_packet.src_addr != src_addr);

              if (!delete_cond) {  // don't forward delete tokens
                write_data_iopipe::write(write_data_packet);
              }

              // flush token 
              if (write_data_packet.user == flush_token_user_id || 
                  delete_cond) {
  
                stop_read_ctrl_pipe::write(1);
                stop_write_ctrl_pipe::write(1);
                stop_read_data_pipe::write(1);
                stop_read_ctrl_user_pipe::write(1);
                stop_write_ctrl_user_pipe::write(1);
                stop_state_manager_pipe::write(1);
                break;
              }
              
              total_credits--;

            }

          } 

          if (total_credits < max_credits) { 
            // read new credit
            new_credit = credit_pipe::read(credit_valid);

            if (credit_valid) {
              total_credits += 1;
            }
          }


        }

      });

    });

    q.submit([&](handler &h) {

      h.single_task<class read_data_credits_handler> ([=]() { 

        bool read_data_valid = false;

        packet_data read_data_packet;
        packet_ctrl write_ctrl_packet;

        uint8_t read_stop;
        bool stop_valid = false;

        while(1) {

          read_stop = stop_read_data_pipe::read(stop_valid);

          if (stop_valid) { 
            break;
          }

          read_data_packet = read_data_iopipe::read(read_data_valid); // block until data comes in
          
          if (read_data_valid) {

            if (read_data_packet.user != flush_token_user_id) { // ack non flush tokens
              write_ctrl_packet.dest_addr = read_data_packet.src_addr;
              write_ctrl_packet.user = ack_data_user_id; // reserved for ack
              write_ctrl_packet.src_addr = src_addr; 
              write_ctrl_packet.payload = 0; // don't care

              write_ack_data_pipe::write(write_ctrl_packet); // ack
            }

            read_data_telemetry_pipe::write(read_data_packet); // block until successful write
          }

        }

      });

    });

    q.submit([&](handler &h) {

      h.single_task<class read_ctrl_credits_handler> ([=]() { 

        packet_ctrl read_ctrl_packet;
        bool read_ctrl_valid = false;

        uint8_t read_stop;
        bool stop_valid = false;

        while(1) {

          read_stop = stop_read_ctrl_pipe::read(stop_valid);

          if (stop_valid) { 
            break;
          }    

          read_ctrl_packet = read_ctrl_iopipe::read(read_ctrl_valid);

          if (read_ctrl_valid) {

            if (read_ctrl_packet.user == ack_data_user_id) { // credit ack, no need to forward to user kernel
              credit_pipe::write(1); // block until successful write
            }
            else {
              read_ctrl_user_pipe::write(read_ctrl_packet); // forward to user kernel
            }

          }

        }

      });

    });

    q.submit([&](handler &h) {

      h.single_task<class write_ctrl_credits_handler> ([=]() { 

        packet_ctrl ack_data_packet;
        bool ack_data_packet_valid;

        packet_ctrl user_packet;
        bool user_packet_valid;

        packet_ctrl write_ctrl_packet;
        bool send_packet = false;

        uint8_t read_stop;
        bool stop_valid = false;



        while(1) {

          read_stop = stop_write_ctrl_pipe::read(stop_valid);
          
          if (stop_valid) { 
            break;
          }

          user_packet = write_ctrl_user_pipe::read(user_packet_valid);

          if (user_packet_valid) {
            write_ctrl_packet = user_packet;
            send_packet = true;
          }

          // lowest priority to data acks
          else if (user_packet_valid == false) {

            ack_data_packet = write_ack_data_pipe::read(ack_data_packet_valid);

            if (ack_data_packet_valid) {
              write_ctrl_packet = ack_data_packet;
              send_packet = true;
            }
          }

          if (send_packet) {
            write_ctrl_iopipe::write(write_ctrl_packet);
            send_packet = false;
          }
        }

      });

    });
    
  }

}
