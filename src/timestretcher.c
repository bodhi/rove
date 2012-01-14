#include "types.h"

#include <stdlib.h>
#include <stdio.h>

#define BUFFER_SIZE 64

const float *const *data(timestretcher *t, ringbuffer **r) {
  int ch;
  for (ch = 0; ch < t->channels; ++ch)
    t->data[ch] = r[ch]->data + r[ch]->read_head;
  return (const float *const*)t->data;
}

void advance(timestretcher *t, sf_count_t count) {
  int ch;
  for (ch = 0; ch < t->channels; ++ch)
    ringbuffer_read_advance(t->source[ch], count);
}

void process(timestretcher *t, sf_count_t size) {
  sf_count_t available = ringbuffer_read_available(t->source[0]);
//  printf("processing %lld out of %lld\n", size, available);
  if (available < size) size = available;
//  printf("really processing %lld\n", size);
  rubberband_process(t->rb, data(t, t->source), size, 0);
  advance(t, size);
}

timestretcher *timestretcher_init(float *interleaved_source, sf_count_t frames, sf_count_t channels, sf_count_t sample_rate) {
  int ch, fr;
  timestretcher *t = calloc(sizeof(timestretcher), 1);
  t->source = calloc(sizeof(ringbuffer *), channels);
  t->scratch = calloc(sizeof(ringbuffer *), channels);
  t->channels = channels;

  t->data = calloc(sizeof(float *), channels);

  // deinterleave into channels
  for (ch = 0; ch < channels; ++ch) {
    t->source[ch] = ringbuffer_init(frames);
    t->scratch[ch] = ringbuffer_init(BUFFER_SIZE);
    for (fr = 0; fr < frames; ++fr) {
      ringbuffer_write(t->source[ch], interleaved_source + (fr * channels) + ch, 1);
    }
  }

//  rubberband_set_default_debug_level(1);
  t->rb = rubberband_new(sample_rate, channels, RubberBandOptionProcessRealTime, 1.05, 1.0);
  rubberband_set_max_process_size(t->rb, frames);

//  process(t, frames);
  rubberband_reset(t->rb);
     
  return t;
}

void timestretcher_free(timestretcher *t) {
  int ch;
  for (ch = 0; ch < t->channels; ++ch) {
    ringbuffer_delete(t->source[ch]);
    ringbuffer_delete(t->scratch[ch]);
  }
  free(t);
}

void buffer(timestretcher *t, double speed) {
//  printf("buffer has %lld space  %g\n", ringbuffer_read_available(t->scratch[0]), speed);
  if (ringbuffer_read_available(t->scratch[0]) == BUFFER_SIZE) { /* back at start */
//    printf("no data, buffering\n");
//    printf("%d samples available\n", available);
    rubberband_set_time_ratio(t->rb, speed);
    while (rubberband_available(t->rb) < BUFFER_SIZE) {
//      int reqd = rubberband_get_samples_required(t->rb);
//      printf("reprocessing %u\n", reqd);
//      printf("behind by %u\n", rubberband_get_latency(t->rb));
//      rubberband_reset(t->rb);
      process(t, BUFFER_SIZE);
    }

    size_t read = rubberband_retrieve(t->rb, (float *const *)data(t, t->scratch), BUFFER_SIZE);
    if (read != BUFFER_SIZE) printf("not enough bytes!\n");
  }
}

void timestretcher_read(timestretcher *t, float *output, sf_count_t frames, double speed) {
  buffer(t, speed);
  if (frames > BUFFER_SIZE) frames = BUFFER_SIZE;
  ringbuffer_read(t->scratch[0], output, frames);
  ringbuffer_read(t->scratch[1], output + 1, frames);
}

sf_count_t timestretcher_position(timestretcher *t) {
  return t->source[0]->read_head;
}

void timestretcher_seek(timestretcher *t, sf_count_t pos) {
  printf("\t\t\tseek to %lld\n", pos);
  rubberband_reset(t->rb);
  int ch;
  for (ch = 0; ch < t->channels; ++ch)
    ringbuffer_read_seek(t->source[ch], pos);
}

void timestretcher_narrow(timestretcher *t, sf_count_t start, sf_count_t end) {
  int ch;
  for (ch = 0; ch < t->channels; ++ch)
    ringbuffer_read_narrow(t->source[ch], start, end);
}
