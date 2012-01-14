#include "types.h"

typedef struct resampler {
  ringbuffer *source;
  SRC_STATE *src;
  sf_count_t channels;
} resampler;

resampler *resampler_init(float *interleaved_source, sf_count_t frames, sf_count_t channels);
void resampler_free(resampler *);
void resampler_read(resampler *, float *output, sf_count_t frames, double speed);
sf_count_t resampler_position(resampler *);
void resampler_seek(resampler *, sf_count_t frame);
void resampler_narrow(resampler *, sf_count_t start, sf_count_t end);
