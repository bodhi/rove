#include "types.h"
//#include "ringbuffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Ringbuffer, copied from another project of mine, hacked together to
provide the ability to narrow the read region (to support looping
sub-sections of the buffer. Narrowing the buffer is probably not at
all thread-safe. */
ringbuffer *ringbuffer_init(sf_count_t floatcount) {
    ringbuffer *buffer;
    buffer = calloc(1, sizeof(ringbuffer));
    buffer->size = floatcount;
    buffer->data = calloc(floatcount, sizeof(float));

    buffer->read_head = 0;
    buffer->write_head = 0;

    ringbuffer_read_narrow(buffer, 0, floatcount);

    return buffer;
}

void ringbuffer_delete(ringbuffer *buffer) {
  free(buffer->data);
  free(buffer);
}

// Yeah, yeah.
int ringbuffer_data_size(ringbuffer *buffer) {
  return -1;
}

// Write is probably buggy when wrapping around, especially since ringbuffer_data_size isn't implemented
void ringbuffer_write(ringbuffer *buffer, float *data, sf_count_t data_size) {
    sf_count_t wrap_amount = data_size - (buffer->size - buffer->write_head);
    if (data_size + ringbuffer_data_size(buffer) >= buffer->size) {
	printf("WARNING: writing past read_head!\n");
	if (data_size > buffer->size) {
	    printf("ERROR: tried to write %lld bytes, but buffer is only %lld bytes\n", data_size, buffer->size);
	}
    }
    if (wrap_amount < 0) {
	wrap_amount = 0;
    }
    memcpy((buffer->data + buffer->write_head), data, (data_size - wrap_amount) * sizeof(float));
    if (wrap_amount > 0) {
      memcpy((buffer->data), data, wrap_amount * sizeof(float));
    }
    
    buffer->write_head = (buffer->write_head + data_size) % buffer->size;
}

// Given 0....s----e....max,
// or 0--e....s--max,
// wrap val to the interval s---e
static sf_count_t mod(sf_count_t val, sf_count_t s, sf_count_t e, sf_count_t max) {
  while (e < s) e += max; // convert 0--e...s--max to 0.....s--max--e
  while (val < max) val += max;
  val -= s; // transpose to range space
  val %= max; // reset max back to 0
  val %= e - s; // wrap to buffer range
  val += s; // transpose back to buffer space
  val %= max; // wrap back into buffer
  return val;
}

// Advance read head by c frames and make sure that it is still in a valid position
void read_advance(ringbuffer *buffer, sf_count_t c) {
  buffer->read_head += c;
  buffer->read_head = mod(buffer->read_head, buffer->start, buffer->end, buffer->size);
}

// for 0...s-*-e...M read from * to e  
// for 0-*-e...s---M read from * to e
// for 0---e...s-*-M read from * to M
sf_count_t read_available(ringbuffer *buffer) {
  sf_count_t available;
  read_advance(buffer, 0); // make sure read head is in valid range
  if (buffer->end < buffer->start && buffer->read_head >= buffer->start) // |--e....s--*--|
    available = buffer->size - buffer->read_head;
  else // |-*-e....s----|
    available = buffer->end - buffer->read_head; 

  return available;
}

// Read as much as possible without needing to reset read_head (ie. wrap the buffer)
sf_count_t ringbuffer_read_segment(ringbuffer *buffer, float *output, sf_count_t read_size) {
  sf_count_t c = read_available(buffer);
  if (c <= read_size) read_size = c;
  
  memcpy(output, buffer->data + buffer->read_head, read_size * sizeof(float));
  read_advance(buffer, read_size);
  
  return read_size;
}

// Read c bytes into out, looping around the buffer if necessary
int ringbuffer_read(ringbuffer *buffer, float *out, sf_count_t c) {
  sf_count_t read;
  while (c > 0) {
    read = ringbuffer_read_segment(buffer, out, c);
    c -= read;
    out += read;
  }
  return 0;
}


void ringbuffer_read_seek(ringbuffer *buffer, sf_count_t i) {
  buffer->read_head = i;
}

// Restrict range of read buffer
void ringbuffer_read_narrow(ringbuffer *buffer, sf_count_t start, sf_count_t end) {
  buffer->start = start;
  buffer->end = end;
}

