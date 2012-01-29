#include "types.h"
#include "ringbuffer.h"
#include "resampler.h"

#include <stdlib.h>
#include <stdio.h>

#define resampler_d(d) ((resampler *)(d->data))

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

void resampler_read(resampler *r, float *output, sf_count_t frames, double speed) {
  src_callback_read(r->src, speed, frames, output);
}

static void resampler_write(data_source_t *d, jack_default_audio_sample_t **buffers, int channels, jack_nframes_t nframes, jack_nframes_t sample_rate, float speed, float volume) {
  float b[2];
  int i;
  //speed = 1.0; //(sample_rate / (double) r->sample_rate) * (1 / speed);

  for( i = 0; i < nframes; i++ ) {
    resampler_read(resampler_d(d), b, 1, speed);
    buffers[0][i] += b[0] * volume;
    buffers[1][i] += b[1] * volume;
  }
}

void resampler_free(data_source_t *d) {
  resampler *r = resampler_d(d);
  ringbuffer_delete(r->source);
  free(r);
  free(d);
}

sf_count_t resampler_position(data_source_t *d) {
  resampler *r = resampler_d(d);
  return r->source->read_head/r->channels;
}

void resampler_seek(data_source_t *d, sf_count_t frame) {
  resampler *r = resampler_d(d);
  ringbuffer_read_seek(r->source, frame * r->channels);
}

void resampler_narrow(data_source_t *d, sf_count_t start, sf_count_t end) {
  resampler *r = resampler_d(d);
  ringbuffer_read_narrow(r->source, start * r->channels, end * r->channels);
}

data_source_t *resampler_data_source(float *interleaved_source, sf_count_t frames, sf_count_t channels) {
    data_source_t *source = calloc(1, sizeof(data_source_t));

  source->write_fn = resampler_write;
  source->seek_fn = resampler_seek;
  source->narrow_fn = resampler_narrow;
  source->position_fn = resampler_position;

  source->data = resampler_init(interleaved_source, frames, channels);

  return source;

}
