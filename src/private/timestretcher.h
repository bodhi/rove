#include "types.h"

#include <rubberband/rubberband-c.h>

typedef struct timestretcher {
  ringbuffer **source;
  ringbuffer **scratch;
  RubberBandState rb;
  sf_count_t channels;
  float **data;
} timestretcher;

timestretcher *timestretcher_init(float *interleaved_source, sf_count_t frames, sf_count_t channels, sf_count_t sample_rate);
void timestretcher_free(timestretcher *);
void timestretcher_read(timestretcher *, float *output, sf_count_t frames, double speed);
sf_count_t timestretcher_position(timestretcher *);
void timestretcher_seek(timestretcher *, sf_count_t frame);
void timestretcher_narrow(timestretcher *, sf_count_t start, sf_count_t end);
