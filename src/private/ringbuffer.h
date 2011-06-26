#include "types.h"
//typedef long long int sf_count_t;

// ----------------------------------------
// Ringbuffer
typedef struct ringbuffer {
  sf_count_t read_head;
  sf_count_t write_head;
  sf_count_t size;
  sf_count_t start;
  sf_count_t end;

  float *data;
} ringbuffer;

ringbuffer *ringbuffer_init(sf_count_t floatcount);
int ringbuffer_data_size(ringbuffer *buffer);
void ringbuffer_debug(ringbuffer *buffer);
void ringbuffer_write(ringbuffer *buffer, float *data, sf_count_t data_size);
int ringbuffer_read(ringbuffer *buffer, float *data, sf_count_t);
void ringbuffer_read_seek(ringbuffer *, sf_count_t);
void ringbuffer_read_narrow(ringbuffer *, sf_count_t start, sf_count_t end);
void ringbuffer_delete(ringbuffer *buffer);
