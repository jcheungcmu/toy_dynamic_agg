#include "cam.h"

#include <sycl/ext/intel/fpga_extensions.hpp>

#ifndef CAM_G
#define CAM_G (32)
#endif

// #define CLOG2_CAM_G __builtin_ctz(CAM_G)
constexpr int CLOG2(int x) {
  int result = 1;
  int power = 0;
  while (x > result) {
    result <<= 1;
    power++;
  }
  return power;
}

#define CLOG2_CAM_G CLOG2(CAM_G)
#define WINDOW_SIZE (CAM_G * 8)

template <typename T>
constexpr T CAM_EXTRA_REGS(T value) {
  #pragma unroll
  for (int i = 0; i < (CLOG2(CAM_G / 64)); i++) {
    value = ext::intel::fpga_reg(value);
  }
  return value;
}

struct insert_info_t {
  bool insert;
  uint16_t insert_idx;
  cam_key_t insert_key;
};

struct evict_info_t {
  bool evict;
  uint16_t evict_idx;
};

// Tune these to get II = 1 and desired FMAX
#ifndef INSERT_REG_DEPTH
#define INSERT_REG_DEPTH (6)
#endif

#ifndef EVICT_REG_DEPTH
#define EVICT_REG_DEPTH (8)
#endif


extern "C" {
  void cam() {

    q.submit([&](handler &h) { h.single_task<class Cam>([=]() { 

      [[maybe_unused]] uint64_t cycle = 0;

      [[intel::fpga_register]] cam_key_t key_memory[CAM_G];
      [[intel::fpga_register]] bool key_valid[CAM_G] = {false};
      [[intel::fpga_register]] cam_value_t value_memory[CAM_G];

      [[intel::fpga_register]] uint16_t count_memory[CAM_G];
      uint16_t count_overflow = 0;
      uint16_t window_count = 0;
      tuple_flit_t in_flit;

      insert_info_t insert[INSERT_REG_DEPTH];
    #pragma unroll
      for (int i = 0; i < INSERT_REG_DEPTH; i++) {
        insert[i].insert = false;
      }

      evict_info_t evict[EVICT_REG_DEPTH];
    #pragma unroll
      for (int i = 0; i < EVICT_REG_DEPTH; i++) {
        evict[i].evict = false;
      }

      [[intel::initiation_interval(1)]] while (true) {
        cycle++;

        // Check if a key is there in more than one entry
        // for (int i = 0; i < CAM_G; i++) {
        //     for (int j = i + 1; j < CAM_G; j++) {
        //         if (key_valid[i] && key_valid[j] && (key_memory[i] ==
        //         key_memory[j]))
        //         {
        //             sycl::ext::oneapi::experimental::printf("@%lu Warning: Key %u
        //             is present in multiple CAM entries: %u and %u\n",
        //                 cycle, key_memory[i], i,  j);
        //         }
        //     }
        // }

        // Shift register
    #pragma unroll
        for (int i = INSERT_REG_DEPTH - 1; i > 0; i--) {
          insert[i] = insert[i - 1];
        }

    #pragma unroll
        for (int i = EVICT_REG_DEPTH - 1; i > 0; i--) {
          evict[i] = evict[i - 1];
        }

        bool read_valid = false;
        in_flit = input_tuples_pipe::read(read_valid);  // Read from input pipe

        ///////////////////////////////////////////////////////////////////////
        // Lookup
        ///////////////////////////////////////////////////////////////////////
        bool match[CAM_G] = {false};
        if (read_valid) {
          window_count = (window_count + 1) % WINDOW_SIZE;

    #pragma unroll
          for (int i = 0; i < CAM_G; i++) {
            if (key_valid[i] && (key_memory[i] == CAM_EXTRA_REGS(in_flit.key))) {
              match[i] = true;
            }

            value_memory[i] += match[i] ? CAM_EXTRA_REGS(in_flit.value) : 0;
            count_memory[i] += match[i] ? 1 : 0;
          }
        }

        bool no_match = read_valid;
    #pragma unroll
        for (int i = 0; i < CAM_G; i++) {
          no_match = no_match && !match[i];
        }

        ///////////////////////////////////////////////////////////////////////
        // Overflow
        ///////////////////////////////////////////////////////////////////////
        if (no_match) {
          tuple_flit_t out_flit;
          out_flit.key = in_flit.key;
          out_flit.value = in_flit.value;
          out_flit.user = raw_token_user_id;
          count_overflow++;
          overflow_tuples_pipe::write(out_flit);  // Write to output pipe
        }

        if ((in_flit.user == flush_token_user_id || in_flit.user == delete_token_user_id || in_flit.user == end_of_stream_token_user_id) && read_valid) {
          break;
        }

        ///////////////////////////////////////////////////////////////////////
        // Insertion Check
        ///////////////////////////////////////////////////////////////////////
        // This must happen before insertion so that we don't insert the key
        // again since no_match is determined during lookup and the new
        // insertion information will be missing during the check
        // Alternatively, we can do it after insertion but check whether the
        // inserted key is the same as the input key that just missed

        // If no match, find an empty slot
        bool empty[(CLOG2_CAM_G + 1) * CAM_G] = {false};
        uint16_t insert_idx[(CLOG2_CAM_G + 1) * CAM_G] = {0};

    #pragma unroll
        for (int i = 0; i < CLOG2_CAM_G; i++) {
          if (i == 0) {
    #pragma unroll
            for (int j = 0; j < CAM_G; j++) {
              empty[j] = !key_valid[j];
              insert_idx[j] = j;
            }
          }
    #pragma unroll
          for (int j = 0; j < (1 << (CLOG2_CAM_G - 1 - i)); j++) {
            uint16_t idx1 = j * 2;
            uint16_t idx2 = j * 2 + 1;
            if (empty[i * CAM_G + idx1]) {
              empty[(i + 1) * CAM_G + j] = true;
              insert_idx[(i + 1) * CAM_G + j] = insert_idx[i * CAM_G + idx1];
            } else if (empty[i * CAM_G + idx2]) {
              empty[(i + 1) * CAM_G + j] = true;
              insert_idx[(i + 1) * CAM_G + j] = insert_idx[i * CAM_G + idx2];
            } else {
              empty[(i + 1) * CAM_G + j] = false;
            }
          }
        }

        insert[0].insert = empty[CLOG2_CAM_G * CAM_G] && no_match;
        insert[0].insert_idx = insert_idx[CLOG2_CAM_G * CAM_G];
        insert[0].insert_key = in_flit.key;

        ///////////////////////////////////////////////////////////////////////
        // Eviction Check
        ///////////////////////////////////////////////////////////////////////
        // Make sure we don't evict an entry when there is still an empty slot
        // since otherwise we might insert the same key twice if the second
        // eviction happens between when an insertion was decided and when it
        // actually happens.

        // Find the least used entry using a tree since SYCL does not infer
        // it automatically from a simple loop search
        uint16_t min_count_idx[(CLOG2_CAM_G + 1) * CAM_G] = {0};
        uint16_t min_count_valid[(CLOG2_CAM_G + 1) * CAM_G] = {0};
        uint16_t min_count[(CLOG2_CAM_G + 1) * CAM_G] = {0xFFFF};
        bool current_evict = false;
    #pragma unroll
        for (int i = 0; i < CLOG2_CAM_G; i++) {
          if (i == 0) {
    #pragma unroll
            for (int j = 0; j < CAM_G; j++) {
              min_count[i * CAM_G + j] = count_memory[j];
              min_count_idx[i * CAM_G + j] = j;
              min_count_valid[i * CAM_G + j] = key_valid[j];
            }
          }
    #pragma unroll
          for (int j = 0; j < (1 << (CLOG2_CAM_G - 1 - i)); j++) {
            uint16_t idx1 = j * 2;
            uint16_t idx2 = j * 2 + 1;
            if ((min_count[i * CAM_G + idx1] < min_count[i * CAM_G + idx2]) || !min_count_valid[i * CAM_G + idx1]) {
              min_count[(i + 1) * CAM_G + j] = min_count[i * CAM_G + idx1];
              min_count_idx[(i + 1) * CAM_G + j] = min_count_idx[i * CAM_G + idx1];
              min_count_valid[(i + 1) * CAM_G + j] = min_count_valid[i * CAM_G + idx1];
            } else {
              min_count[(i + 1) * CAM_G + j] = min_count[i * CAM_G + idx2];
              min_count_idx[(i + 1) * CAM_G + j] = min_count_idx[i * CAM_G + idx2];
              min_count_valid[(i + 1) * CAM_G + j] = min_count_valid[i * CAM_G + idx2];
            }
          }
        }
        current_evict = (min_count[CLOG2_CAM_G * CAM_G] < count_overflow) && (window_count == 0) &&
                        min_count_valid[CLOG2_CAM_G * CAM_G];
        evict[0].evict = current_evict;
        evict[0].evict_idx = min_count_idx[CLOG2_CAM_G * CAM_G];

        ///////////////////////////////////////////////////////////////////////
        // Insertion
        ///////////////////////////////////////////////////////////////////////
        // Update based on delayed insert signals
        if (insert[INSERT_REG_DEPTH - 1].insert) {
          // This loop tries to decouple the index check and make each
          // entry's circuit independent from each other for parallelism
          // A mux tree for selecting key_valid[idx] would not scale to large
          // CAM_G values and this loop should break that up - the idx to one
          // hot conversion can be pipelined which makes the remaining logic
          // O(1)

    #pragma unroll
          for (int i = 0; i < CAM_G; i++) {
            if (!key_valid[i] && (i == insert[INSERT_REG_DEPTH - 1].insert_idx)) {
              key_memory[i] = insert[INSERT_REG_DEPTH - 1].insert_key;
              key_valid[i] = true;

              value_memory[i] = 0;
              // Prevent the newly inserted entry from being evicted
              // immediately - give it a chance?
              // count_memory[i] = window_count;
              count_memory[i] = 0;

              // sycl::ext::oneapi::experimental::printf("@%lu Inserting key: %u at
              // idx: %u\n",
              //     cycle,
              //     insert[INSERT_REG_DEPTH - 1].insert_key,
              //     insert[INSERT_REG_DEPTH - 1].insert_idx);
            }
          }
        }

        ///////////////////////////////////////////////////////////////////////
        // Eviction
        ///////////////////////////////////////////////////////////////////////
        // Perform eviction based on delayed evict signals
        if (evict[EVICT_REG_DEPTH - 1].evict) {
          // sycl::ext::oneapi::experimental::printf("@%lu Evicting key: %u at idx:
          // %u with value: %u and count: %u\n",
          //     cycle,
          //     key_memory[evict[EVICT_REG_DEPTH - 1].evict_idx],
          //     evict[EVICT_REG_DEPTH - 1].evict_idx,
          //     value_memory[evict[EVICT_REG_DEPTH - 1].evict_idx],
          //     count_memory[evict[EVICT_REG_DEPTH - 1].evict_idx]);
          tuple_flit_t out_flit;
          out_flit.key = CAM_EXTRA_REGS(key_memory[evict[EVICT_REG_DEPTH - 1].evict_idx]);
          out_flit.value = CAM_EXTRA_REGS(value_memory[evict[EVICT_REG_DEPTH - 1].evict_idx]);
          out_flit.user = raw_token_user_id;
          evict_tuples_pipe::write(out_flit);  // Write to output pipe

          // Invalidate the evicted entry
          key_valid[evict[EVICT_REG_DEPTH - 1].evict_idx] = false;
        }

        // Zero out count memory periodically
        if (window_count == 0) {
    #pragma unroll
          for (int i = 0; i < CAM_G; i++) {
            count_memory[i] = 0;
          }
          count_overflow = 0;
        }
      }

      if (in_flit.user == end_of_stream_token_user_id) {
        // Send EOF flit
        tuple_flit_t eof_flit;
        eof_flit.key = 0;
        eof_flit.value = 0;
        eof_flit.user = end_of_stream_token_user_id;
        overflow_tuples_pipe::write(eof_flit);
        evict_tuples_pipe::write(eof_flit);

        flush_pipe::read(flush_pipe_valid);

        if (flush_pipe_valid) { 
          // Flush remaining entries in CAM
          for (int i = 0; i < CAM_G; i++) {
            if (CAM_EXTRA_REGS(key_valid[i])) {
              tuple_flit_t out_flit;
              out_flit.key = CAM_EXTRA_REGS(key_memory[i]);
              out_flit.value = CAM_EXTRA_REGS(value_memory[i]);
              out_flit.user = raw_token_user_id;
              overflow_tuples_pipe::write(out_flit);
            }
          }

          // Send EOF flit
          tuple_flit_t eof_flit;
          eof_flit.key = 0;
          eof_flit.value = 0;
          eof_flit.user = in_flit.user; // propagate the token type (flush or delete)
          overflow_tuples_pipe::write(eof_flit);
          evict_tuples_pipe::write(eof_flit);
        }


      }

      else if (in_flit.user == flush_token_user_id || in_flit.user == delete_token_user_id) {

      // Flush remaining entries in CAM
      for (int i = 0; i < CAM_G; i++) {
        if (CAM_EXTRA_REGS(key_valid[i])) {
          tuple_flit_t out_flit;
          out_flit.key = CAM_EXTRA_REGS(key_memory[i]);
          out_flit.value = CAM_EXTRA_REGS(value_memory[i]);
          out_flit.user = raw_token_user_id;
          overflow_tuples_pipe::write(out_flit);
        }
      }

      // Send EOF flit
      tuple_flit_t eof_flit;
      eof_flit.key = 0;
      eof_flit.value = 0;
      eof_flit.user = in_flit.user; // propagate the token type (flush or delete)
      overflow_tuples_pipe::write(eof_flit);
      evict_tuples_pipe::write(eof_flit);

    }); });
  }

  void merge() {
    q.submit([&](handler &h) { h.single_task<class Merge>([=]() { 

      bool evict_eof = false, overflow_eof = false;
      while (true) {
        bool valid = false;
        tuple_flit_t evict_flit = evict_tuples_pipe::read(valid);
        evict_eof = evict_eof || (valid && evict_flit.eof);
        evict_flit.eof = false;
        if (evict_eof && overflow_eof) {
          evict_flit.eof = true;
          evict_eof = false;
          overflow_eof = false;
        }

        if (valid) {
          output_tuples_pipe::write(evict_flit);
        } else {
          valid = false;
          tuple_flit_t overflow_flit = overflow_tuples_pipe::read(valid);
          overflow_eof = overflow_eof || (valid && overflow_flit.eof);
          overflow_flit.eof = false;

          if (evict_eof && overflow_eof) {
            overflow_flit.eof = true;
            evict_eof = false;
            overflow_eof = false;
          }

          if (valid) output_tuples_pipe::write(overflow_flit);
        }
      }

    }); });
  }

}