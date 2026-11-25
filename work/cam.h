#pragma once

#include "cam_types.h"
#include <sycl/sycl.hpp>
using namespace sycl;

struct tuple_flit_t {
  cam_key_t key;
  cam_value_t value;
  uint8_t user;
};

using input_tuples_pipe = ext::intel::pipe<class input_tuples_pipe_id, tuple_flit_t, DEFAULT_PIPE_DEPTH>;
using flush_pipe = ext::intel::pipe<class flush_pipe_id, tuple_flit_t, DEFAULT_PIPE_DEPTH>;
using evict_tuples_pipe = ext::intel::pipe<class evict_tuples_pipe_id, tuple_flit_t, DEFAULT_PIPE_DEPTH>;
using overflow_tuples_pipe = ext::intel::pipe<class overflow_tuples_pipe_id, tuple_flit_t, DEFAULT_PIPE_DEPTH>;
using output_tuples_pipe = ext::intel::pipe<class output_tuples_pipe_id, tuple_flit_t, DEFAULT_PIPE_DEPTH>;

extern "C" {
  
}
SYCL_EXTERNAL void cam();
SYCL_EXTERNAL void merge();
