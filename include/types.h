#ifndef TYPES_H
#define TYPES_H

template <unsigned ID>
struct pipe_id {
  static constexpr unsigned id = ID;
};

struct pipe_id_0 {
  static constexpr unsigned id = 0;
};

struct pipe_id_1 {
  static constexpr unsigned id = 1;
};

struct pipe_id_2 {
  static constexpr unsigned id = 2;
};

struct pipe_id_3 {
  static constexpr unsigned id = 3;
};

struct key_value {
  uint32_t key[16];
  uint32_t value[16];
};

struct packet_ctrl{
  uint8_t dest_addr; // position of dest_addr is important for extraction to noc
  uint8_t src_addr;
  uint8_t user;
  uint8_t payload;
};

struct packet_data{
  uint8_t dest_addr; // dest_addr is expected to be the first field
  uint8_t src_addr;
  uint8_t user;
  uint8_t pad;
  1024bits_type payload;
};

// struct pr_request_packet{
//   uint8_t module_id;
//   uint8_t slot_addr;
// };

#endif
