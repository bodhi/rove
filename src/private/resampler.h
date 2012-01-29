#include "types.h"

typedef struct resampler {
  ringbuffer *source;
  SRC_STATE *src;
  sf_count_t channels;
} resampler;

data_source_t *resampler_data_source(float *interleaved_source, sf_count_t frames, sf_count_t channels);
