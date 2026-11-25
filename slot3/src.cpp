#include "slot3.h"

extern "C" {

  event slot3_src(queue &q, buffer<key_value>& input_buf, size_t num_items) {

    return q.submit([&](handler &h) {
      // Create an accessor with read permission.
      accessor input(input_buf, h, read_only);

      h.single_task<class src_task> ([=]() { 

        packet_data write_data_packet;
        write_data_packet.dest_addr = 0; // will be overwritten
        write_data_packet.src_addr = 0; // will be overwritten 

        key_value data; 

        uint8_t pr_handler_ack;
        bool pr_handler_ack_valid;

        size_t i;

        for (i = 0; i < num_items; i++) {

          data = input[i];

          write_data_packet.user = raw_token_user_id; 
          write_data_packet.payload = data; 
          src_to_agg_pipe::write(write_data_packet);

        }
        
        if (i == num_items) {

          // send end of stream token first to close PR handler 
          write_data_packet.user = end_of_stream_token_user_id; // end of stream token
          write_data_packet.payload = {0,0}; // payload is don't care
          src_to_agg_pipe::write(write_data_packet);

          // wait for OK from PR handler then clean up everything
          while(1) {
            pr_handler_ack = pr_handler_to_src_pipe::read(pr_handler_ack_valid); 

            if (pr_handler_ack_valid) {

              write_data_packet.user = flush_token_user_id; // flush token
              write_data_packet.payload = {0,0}; // payload is don't care
              src_to_agg_pipe::write(write_data_packet);
              break;
            }
          }

        }


      });

    });
  }
  
  event slot3_agg(queue &q) {

    return q.submit([&](handler &h) {

      h.single_task<class agg_task> ([=]() { 

        packet_data write_data_packet;
        packet_data read_data_packet;

        write_data_packet.dest_addr = 0; // will be overwritten
        write_data_packet.src_addr = 0; // will be overwritten

        key_value local_table;

        uint8_t isFirstKey = 1;

        while(1) {

          read_data_packet = src_to_agg_pipe::read();

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
  
  // void slot3_cam_shim(queue &q) {

  //   q.submit([&](handler &h) {

  //     h.single_task<class cam_input_shim> ([=]() { 

  //       packet_data read_data_packet;
  //       tuple_flit_t cam_input_flit;

  //       while (1) {
  //         read_data_packet = src_to_agg_pipe::read();

  //         cam_input_flit.key = read_data_packet.payload.key;
  //         cam_input_flit.value = read_data_packet.payload.value;

  //         if (read_data_packet.user == raw_token_user_id) {
  //           cam_input_flit.eof = false;
  //           input_tuples_pipe::write(cam_input_flit);
  //         }
  //         else if (read_data_packet.user == end_of_stream_token_user_id) { 
  //           cam_input_flit.eof = true;
  //           input_tuples_pipe::write(cam_input_flit);
  //         }
  //         else if (read_data_packet.user == flush_token_user_id || 
  //                  read_data_packet.user == delete_token_user_id) {
  //           cam_input_flit.eof = true;
  //           flush_pipe::write(cam_input_flit);
  //         }

  //       }
  //     });

  //   });

  //   q.submit([&](handler &h) {

  //     h.single_task<class cam_output_shim> ([=]() { 

  //       tuple_flit_t cam_output_flit;

  //       packet_data write_data_packet;

  //       bool cam_output_flit_valid = false;
  //       bool read_data_valid = false;


  //       while(1) {
  //         cam_output_flit = output_tuples_pipe::read(cam_output_flit_valid );

  //         if (cam_output_flit_valid) {
  //           write_data_packet.payload.key = cam_output_flit.key;
  //           write_data_packet.payload.value = cam_output_flit.value;

  //           if (cam_output_flit.eof == false) {
  //             write_data_packet.user = raw_token_user_id; 
  //             write_data_user_pipe::write(write_data_packet);
  //           }
  //           else if (cam_output_flit.eof == true) { 
  //             write_data_packet.user = flush_token_user_id; // flush token
  //             write_data_user_pipe::write(write_data_packet);
  //             break;
  //           }
  //         } 
  //       }

  //     });

  //   });
  // }

  event slot3_results_handler (queue &q, buffer<key_value>& result_buf, buffer<uint8_t>& count_buf) {

    return q.submit([&](handler &h) {

      accessor result(result_buf, h, write_only, no_init);
      accessor count_acc(count_buf, h, write_only, no_init);
      
      h.single_task<class results_handler_task> ([=]() { 

        packet_data read_data_packet;

        key_value data; 
        
        uint8_t count = 0;

        while(1) {

            read_data_packet = results_pipe::read();

            if (read_data_packet.user == result_token_user_id) { // results
              data = read_data_packet.payload;

              result[count] = data;
              count++;
            }
            else if (read_data_packet.user == flush_token_user_id) { // done
              count_acc[0] = count;
              stop_read_ctrl_pipe::write(1);
              stop_write_ctrl_pipe::write(1);
              stop_read_data_pipe::write(1);

              stop_read_ctrl_user_pipe::write(1);
              stop_write_ctrl_user_pipe::write(1);
              // stop lock manager 
              stop_lock_manager_pipe::write(1);

              stop_state_manager_pipe::write(1);

              break;
            }

        }
        
      });

    });
  }

  event slot3_overflow_handler (queue &q, buffer<key_value>& overflow_buf, buffer<size_t>& count_buf) { 
    return q.submit([&](handler &h) {

      accessor overflow(overflow_buf, h, write_only, no_init);
      accessor count_acc(count_buf, h, write_only, no_init);

      h.single_task<class overflow_handler> ([=]() { 

        packet_data read_data_packet;
        size_t count = 0; 

        // pr_request_packet request;

        while(1) { 
          read_data_packet = overflow_pipe::read();

          if (read_data_packet.user == raw_token_user_id) { 
            overflow[count] = read_data_packet.payload;
            count++;

            // // notify PR handler
            // request.module_id = read_data_packet.payload.key; 
            // request.slot_addr = 0; // unused, cannot request specific slot in this design

            // overflow_to_pr_request_filter_pipe::write(request);
          }
          // else if (read_data_packet.user == end_of_stream_token_user_id) { 
          //   request.module_id = stop_pr_kernels_module_id; 
          //   request.slot_addr = 0; // unused, cannot request specific slot in this design
    
          //   overflow_to_pr_request_filter_pipe::write(request);
          // }
          else if (read_data_packet.user == flush_token_user_id) { 
            count_acc[0] = count;
            break;
          }
        }
        
      });

    });
  }

  event slot3_fork (queue &q) { 
    return q.submit([&](handler &h) {

      h.single_task<class fork_task> ([=]() { 

        packet_data read_data_packet;

        while(1) { 
          read_data_packet = read_data_user_pipe::read();

          if (read_data_packet.user == raw_token_user_id) { 
            read_data_telemetry_pipe::write(read_data_packet);
          }
          else if (read_data_packet.user == result_token_user_id) { 
            results_pipe::write(read_data_packet);
          }
          else if (read_data_packet.user == end_of_stream_token_user_id) { 
            read_data_telemetry_pipe::write(read_data_packet);
          }
          else if (read_data_packet.user == flush_token_user_id) { 
            results_pipe::write(read_data_packet);
            read_data_telemetry_pipe::write(read_data_packet);
            break;
          }
        }

      });

    });
  }

  void slot3_user_control_handler(queue &q) {
    q.submit([&](handler &h) {

      h.single_task<class write_ctrl_handler> ([=]() { 

        packet_ctrl pr_response_packet;
        bool pr_response_packet_valid = false;

        packet_ctrl pr_request_packet;
        bool pr_request_packet_valid = false;

        packet_ctrl lock_acquire_packet;
        bool lock_acquire_packet_valid = false;

        packet_ctrl lock_release_packet;
        bool lock_release_packet_valid = false;

        packet_ctrl lock_response_packet;
        bool lock_response_packet_valid = false;

        // packet_ctrl update_next_packet;
        // bool update_next_packet_valid = false;

        // packet_ctrl ack_update_next_packet;
        // bool ack_update_next_packet_valid = false;

        packet_ctrl state_manager_request_packet;
        bool state_manager_request_packet_valid = false;

        packet_ctrl state_manager_response_packet;
        bool state_manager_response_packet_valid = false;

        packet_ctrl pr_handler_to_state_manager_request_packet; 
        bool pr_handler_to_state_manager_request_packet_valid = false;

        packet_ctrl state_mgr_to_pr_handler_response_packet;
        bool state_mgr_to_pr_handler_response_packet_valid = false;

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


          pr_response_packet = write_pr_response_pipe::read(pr_response_packet_valid);

          if (pr_response_packet_valid == true) {

            write_ctrl_packet = pr_response_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);

          }

          lock_acquire_packet = write_lock_acquire_pipe::read(lock_acquire_packet_valid);

          if (lock_acquire_packet_valid == true) {

            write_ctrl_packet = lock_acquire_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);
          }

          lock_release_packet = write_lock_release_pipe::read(lock_release_packet_valid);

          if (lock_release_packet_valid == true) {

            write_ctrl_packet = lock_release_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);
          }

          lock_response_packet = write_lock_response_pipe::read(lock_response_packet_valid);

          if (lock_response_packet_valid == true) {

            write_ctrl_packet = lock_response_packet;
            write_ctrl_user_pipe::write(write_ctrl_packet);
          }

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

          pr_handler_to_state_manager_request_packet = write_pr_handler_to_state_mgr_request_pipe::read(pr_handler_to_state_manager_request_packet_valid);
          if (pr_handler_to_state_manager_request_packet_valid == true) {

            write_ctrl_packet = pr_handler_to_state_manager_request_packet;
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
            else if (read_ctrl_packet.user == lock_acquire_user_id) {
              read_lock_acquire_pipe::write(read_ctrl_packet); // forward to lock acquire
            }
            else if (read_ctrl_packet.user == lock_release_user_id) {
              read_lock_release_pipe::write(read_ctrl_packet); // forward to lock release
            }
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

  event slot3_mux(queue &q) {
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

  event slot3_state_manager(queue &q, uint8_t src_addr, uint8_t init_prev_addr, uint8_t init_next_addr) { 
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
  
  event slot3_addr_translation(queue &q, uint8_t init_src_addr, uint8_t init_next_addr) {

    return q.submit([&](handler &h) {

      h.single_task<class write_data_translation_task> ([=]() { 

        packet_data write_data_packet;
        packet_data read_data_packet;

        bool read_data_valid = false;
        uint8_t next_addr = init_next_addr; // always initially set to head addr
        uint8_t src_addr = init_src_addr;

        packet_ctrl update_next_packet;
        bool update_next_valid = false; 

        packet_ctrl write_ctrl_packet;

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
            
            mux1_pipe::write(write_data_packet);

            if (write_data_packet.user == flush_token_user_id) {
              break;
            }
          }
          
        }

        
      });

    });
  }

  void slot3_credits_handler(queue &q, uint8_t src_addr) {

    q.submit([&](handler &h) {

      h.single_task<class write_data_credits_handler> ([=]() { 

        packet_data write_data_packet;
        uint8_t total_credits = max_credits; // init to max credits 
        
        uint8_t new_credit;

        bool read_data_valid = false;

        bool credit_valid = false;

        packet_ctrl read_ctrl_packet;
        bool read_ctrl_valid = false;

        packet_ctrl write_ctrl_packet;

        while(1) {

          if (total_credits > 0) {

              write_data_packet = write_data_with_translated_addr_pipe::read(read_data_valid);


              if (read_data_valid) {

                write_data_iopipe::write(write_data_packet); 

                // flush token 
                if (write_data_packet.user == flush_token_user_id) {
                  // stop_read_ctrl_pipe::write(1);
                  // stop_write_ctrl_pipe::write(1);
                  // stop_read_data_pipe::write(1);
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

            read_data_user_pipe::write(read_data_packet); // block until successful write
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


  event slot3_pr_handler(queue &q, uint8_t src_addr, uint8_t lock_mgr_addr, uint8_t local_state_mgr_addr) { 
    return q.submit([&](handler &h) {

      h.single_task<class pr_handler_task> ([=]() { 

        packet_ctrl request;
        packet_ctrl response_from_host;


        packet_ctrl write_ctrl_packet;
        packet_ctrl read_ctrl_packet;

        while(1) { 

          request = read_pr_request_pipe::read(); 

          if (request.payload == stop_pr_kernels_module_id) {

            // clean up pr_request_kernel
            pr_request_to_host_pipe::write(request);

            // try to acquire lock 
            write_ctrl_packet.dest_addr = lock_mgr_addr; // dest is lock manager addr
            write_ctrl_packet.src_addr = src_addr;
            write_ctrl_packet.user = lock_acquire_user_id; // reserved for update dest
            write_ctrl_packet.payload = 0; // don't care 

            write_lock_acquire_pipe::write(write_ctrl_packet); // send acquire lock packet

            // wait for lock response
            read_ctrl_packet = read_lock_response_pipe::read();

            while (read_ctrl_packet.payload == lock_denied_id) { // lock denied
              // try again
              write_lock_acquire_pipe::write(write_ctrl_packet); // send acquire lock packet

              // wait for lock response
              read_ctrl_packet = read_lock_response_pipe::read();
            }
            
            // tell src to send flush token
            pr_handler_to_src_pipe::write(pr_handler_to_src_id);



            // write_ctrl_packet.dest_addr = local_state_mgr_addr; // dest is previous tail addr
            // write_ctrl_packet.src_addr = src_addr;
            // write_ctrl_packet.user = stop_state_manager_user_id; // update old tail's next pointer
            // write_ctrl_packet.payload = 0; // don't care

            // write_pr_handler_to_state_mgr_request_pipe::write(write_ctrl_packet);
            break;
          }
          else { // valid request

            // try to acquire lock 
            write_ctrl_packet.dest_addr = lock_mgr_addr; // dest is lock manager addr
            write_ctrl_packet.src_addr = src_addr;
            write_ctrl_packet.user = lock_acquire_user_id; // reserved for update dest
            write_ctrl_packet.payload = 0; // don't care 

            write_lock_acquire_pipe::write(write_ctrl_packet); // send acquire lock packet

            // wait for lock response
            read_ctrl_packet = read_lock_response_pipe::read();

            if (read_ctrl_packet.payload == lock_denied_id) { // lock denied
              // send nack to pr_request_filter to try again later
              write_ctrl_packet.dest_addr = request.src_addr; // dest is lock manager addr
              write_ctrl_packet.src_addr = src_addr;
              write_ctrl_packet.user = pr_response_user_id; // reserved for update dest
              write_ctrl_packet.payload = pr_request_fail_id; // don't care 

              write_pr_response_pipe::write(write_ctrl_packet); // send nack packet
            }
            else {

              // lock granted, now ask the host 

              pr_request_to_host_pipe::write(request);

              // block until response
              response_from_host = pr_response_from_host_pipe::read(); 
              if (response_from_host.payload != pr_request_fail_id) { // pr request was successful    


                write_ctrl_packet.dest_addr = local_state_mgr_addr; // dest is previous tail addr
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.user = send_prev_insert_next_user_id; // update old tail's next pointer
                write_ctrl_packet.payload = response_from_host.payload; // new module is at this address

                write_pr_handler_to_state_mgr_request_pipe::write(write_ctrl_packet);
                read_ctrl_packet = read_state_mgr_to_pr_handler_response_pipe::read();
                
                if (read_ctrl_packet.user == state_mgr_to_pr_handler_response_user_id) { 
                  write_ctrl_packet.dest_addr = local_state_mgr_addr; // dest is previous tail addr
                  write_ctrl_packet.src_addr = src_addr;
                  write_ctrl_packet.user = update_prev_from_pr_handler_user_id; // update tail ptr to new tail
                  write_ctrl_packet.payload = response_from_host.payload; // new module is at this address

                  write_pr_handler_to_state_mgr_request_pipe::write(write_ctrl_packet);
                  read_ctrl_packet = read_state_mgr_to_pr_handler_response_pipe::read();
            
                  if (read_ctrl_packet.user == state_mgr_to_pr_handler_response_user_id) { // ack should always come from previous tail

                    // release the lock 
                    write_ctrl_packet.dest_addr = lock_mgr_addr; // dest is previous tail addr
                    write_ctrl_packet.src_addr = src_addr;
                    write_ctrl_packet.user = lock_release_user_id; // reserved for update dest
                    write_ctrl_packet.payload = 0; // don't care

                    write_lock_release_pipe::write(write_ctrl_packet); // send update dest packet
                    
                    // send ack to pr_request_filter 
                    write_ctrl_packet.dest_addr = request.src_addr; // dest is lock manager addr
                    write_ctrl_packet.src_addr = src_addr;
                    write_ctrl_packet.user = pr_response_user_id; // reserved for update dest
                    write_ctrl_packet.payload = pr_request_success_id; // don't care 
                  
                    write_pr_response_pipe::write(write_ctrl_packet); // send ack packet
                  }
                }

              }
              else if (response_from_host.payload == pr_request_fail_id) {

                // release the lock
                write_ctrl_packet.dest_addr = lock_mgr_addr; // dest is previous tail addr
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.user = lock_release_user_id; // reserved for update dest
                write_ctrl_packet.payload = 0; // don't care

                write_lock_release_pipe::write(write_ctrl_packet); // send update dest packet
                
                // send nack to pr_request_filter to try again later
                write_ctrl_packet.dest_addr = request.src_addr; // dest is lock manager addr
                write_ctrl_packet.src_addr = src_addr;
                write_ctrl_packet.user = pr_response_user_id; // reserved for update dest
                write_ctrl_packet.payload = pr_request_fail_id; // don't care 

                write_pr_response_pipe::write(write_ctrl_packet); // send nack packet
              }


            }


          }
  
        }

      });

    });
  }
  

  event pr_request_to_host_kernel(queue &q, buffer<packet_ctrl>& a_buf) {

    return q.submit([&](handler &h) {
      accessor a(a_buf, h, write_only, no_init);

      h.single_task<class pr_request> ([=]() { 

        packet_ctrl read_request;
        read_request = pr_request_to_host_pipe::read();

        a[0] = read_request; // Store the initial request in the first element of the buffer
      });

    });
  }

  event pr_response_from_host_kernel(queue &q, packet_ctrl response) {

    return q.submit([&](handler &h) {

      h.single_task<class pr_ack> ([=]() { 

        pr_response_from_host_pipe::write(response);
      });

    });
  }

  event slot3_telemetry(queue &q) {
    return q.submit([&](handler &h) {

      h.single_task<class telemetry_task> ([=]() { 

        packet_data read_data_packet;
        while(1) { 
          read_data_packet = read_data_telemetry_pipe::read();

          if (read_data_packet.user == raw_token_user_id) { 

            // notify pr handler
            telemetry_to_request_pipe::write(read_data_packet.payload.key);
          }
          else if (read_data_packet.user == end_of_stream_token_user_id) { 
            telemetry_to_request_pipe::write(stop_pr_kernels_module_id);
          }

          overflow_pipe::write(read_data_packet);

          if (read_data_packet.user == flush_token_user_id) { 
            break;
          }
        }
      });

    });
  }


  event slot3_telemetry_request_gen(queue &q, uint8_t pr_handler_addr, uint8_t src_addr) { // telemetry on the overflow rate

    return q.submit([&](handler &h) {

      h.single_task<class telemetry_request_gen> ([=]() { 

        uint8_t telemetry_request;
        bool telemetry_request_valid = false;

        packet_ctrl pr_response;
        bool pr_response_valid = false;

        bool request_in_progress = false;        

        packet_ctrl write_ctrl_packet;

        while(1) { 
          
          telemetry_request = telemetry_to_request_pipe::read(telemetry_request_valid);
          pr_response = read_pr_response_pipe::read(pr_response_valid);

          if (pr_response_valid) {
            request_in_progress = false;
          }

          if (telemetry_request_valid) {

            if (telemetry_request == stop_pr_kernels_module_id) {
              // send PR request even if one is in progress 
              write_ctrl_packet.dest_addr = pr_handler_addr; // dest is previous tail addr
              write_ctrl_packet.src_addr = src_addr;
              write_ctrl_packet.user = pr_request_user_id; 
              write_ctrl_packet.payload = telemetry_request;
              write_pr_request_pipe::write(write_ctrl_packet);

              break;
            }
            else {

              if (request_in_progress == false) {

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
      });

    });
  }


  event lock_manager(queue &q, uint8_t src_addr) { 
    return q.submit([&](handler &h) {

      h.single_task<class lock_manager_task> ([=]() { 

        packet_ctrl acquire;
        packet_ctrl release;
        packet_ctrl lock_response;

        bool acquire_valid = false;
        bool release_valid = false;

        uint8_t read_stop;
        bool stop_valid = false; 

        bool lock_acquired = false;
        
        while(1) { 
          
          read_stop = stop_lock_manager_pipe::read(stop_valid);
          
          if (stop_valid) { 
            break;
          }

          acquire = read_lock_acquire_pipe::read(acquire_valid);
          release = read_lock_release_pipe::read(release_valid);
  
          if (acquire_valid) {
            if (lock_acquired == false) {
              // grant lock
              lock_response.payload = lock_granted_id; // lock granted
              lock_response.user = lock_response_user_id;
              lock_response.src_addr = src_addr;
              lock_response.dest_addr = acquire.src_addr;

              write_lock_response_pipe::write(lock_response);
              lock_acquired = true;
            }
            else {
              // deny lock
              lock_response.payload = lock_denied_id; // lock denied
              lock_response.user = lock_response_user_id;
              lock_response.src_addr = src_addr;
              lock_response.dest_addr = acquire.src_addr;

              write_lock_response_pipe::write(lock_response);
            }
          }

          if (release_valid) {
            lock_acquired = false;
          }

        }

      });

    });
  }


}
