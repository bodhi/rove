#include "types.h"

#include <stdlib.h>
#include <stdio.h>

static long resampler_src_callback(void *cb_data, float **data) {
  resampler *r = cb_data;
  sf_count_t amount;
  
  if( !data )
    return 0;

  amount = ringbuffer_read_available(r->source);
  if (amount > 256) amount = 256;

  *data = r->source->data + r->source->read_head;
  ringbuffer_read_advance(r->source, amount);

  return amount/r->channels;
}

resampler *resampler_init(float *interleaved_source, sf_count_t frames, sf_count_t channels) {
  int err;

  resampler *r = calloc(sizeof(resampler), 1);
  r->source = ringbuffer_init_with_data(interleaved_source, frames * channels);
  ringbuffer_write(r->source, interleaved_source, frames * channels);
  r->src = src_callback_new(resampler_src_callback, SRC_SINC_FASTEST, channels, &err, r);
  r->channels = channels;
  return r;
}

void resampler_free(resampler *r) {
  ringbuffer_delete(r->source);
  free(r);
}

void resampler_read(resampler *r, float *output, sf_count_t frames, double speed) {
  src_callback_read(r->src, speed, frames, output);
}

sf_count_t resampler_position(resampler *r) {
  return r->source->read_head/r->channels;
}

void resampler_seek(resampler *r, sf_count_t frame) {
  ringbuffer_read_seek(r->source, frame * r->channels);
}

void resampler_narrow(resampler *r, sf_count_t start, sf_count_t end) {
  ringbuffer_read_narrow(r->source, start * r->channels, end * r->channels);
}
