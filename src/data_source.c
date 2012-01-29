#include <stdlib.h>

#include "types.h"
#include "ringbuffer.h"

typedef struct simple_data_source simple_data_source_t;

#define simple_d(d) ((simple_data_source_t *)(d->data))
#define for_each_buffer(d, buffer) \
  int _buff_i; \
  ringbuffer *buffer; \
  for(_buff_i = 0, buffer = d->buffers[0]; _buff_i < d->channels; buffer = d->buffers[++_buff_i])

struct simple_data_source {
sf_count_t channels;
ringbuffer **buffers;
};

simple_data_source_t *simple_init(sf_count_t channels, sf_count_t frames) {
  simple_data_source_t *source = malloc(sizeof(simple_data_source_t));
  source->channels = channels;

printf("creating %llu buffers of %llu\n", channels, frames);

source->buffers = calloc(channels, sizeof(ringbuffer *));
int i;
for (i = 0; i < channels; ++i) {
source->buffers[i] = ringbuffer_init(frames);
printf("buffer %d: %p\n", i, source->buffers[i]);
}

return source;
}

static void simple_seek(data_source_t *d, sf_count_t frame) {
  simple_data_source_t *r = simple_d(d);
  for_each_buffer(r, buffer) {
    ringbuffer_read_seek(buffer, frame);
    printf("buffer: %p\n", buffer);
  }
}

static void simple_narrow(data_source_t *d, sf_count_t start, sf_count_t end) {
  simple_data_source_t *r = simple_d(d);
  for_each_buffer(r, buffer) {
    ringbuffer_read_narrow(buffer, start, end);
  }
}

static sf_count_t simple_position(data_source_t *d) {
  simple_data_source_t *r = simple_d(d);
  return r->buffers[0]->read_head;
}

static sf_count_t simple_write_position(data_source_t *d) {
  simple_data_source_t *r = simple_d(d);
  return r->buffers[0]->write_head;
}

static void simple_read(data_source_t *d, jack_default_audio_sample_t **buffers, int channels, jack_nframes_t nframes, jack_nframes_t sample_rate) {
  simple_data_source_t *r = simple_d(d);
int i;
for(i = 0; i < channels; ++i) {
ringbuffer_write(r->buffers[i], buffers[i], nframes);
  }
}

static void simple_write(data_source_t *d, jack_default_audio_sample_t **buffers, int channels, jack_nframes_t nframes, jack_nframes_t sample_rate, float speed, float volume) {
  simple_data_source_t *r = simple_d(d);
int ch, buf;
for (ch = 0; ch < channels; ++ch) {
  for (buf = 0; buf < nframes; ++buf) {
    buffers[ch][buf] = ringbuffer_read_one(r->buffers[ch]) * volume;
  }
}
}

data_source_t *simple_data_source(sf_count_t frames, sf_count_t channels) {
  data_source_t *source = calloc(1, sizeof(data_source_t));
  
  source->read_fn = simple_read;
  source->write_fn = simple_write;
  source->seek_fn = simple_seek;
  source->narrow_fn = simple_narrow;
  source->position_fn = simple_position;
  source->write_position_fn = simple_write_position;

  source->data = simple_init(frames, channels);

  return source;
}
