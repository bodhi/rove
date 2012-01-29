typedef struct data_source data_source_t;

typedef void (*data_source_read_fn_t)(data_source_t *self, jack_default_audio_sample_t **buffers, int channels, jack_nframes_t nframes, jack_nframes_t sample_rate);
typedef void (*data_source_write_fn_t)(data_source_t *self, jack_default_audio_sample_t **buffers, int channels, jack_nframes_t nframes, jack_nframes_t sample_rate, float speed, float volume);
typedef void (*data_source_seek_fn_t)(data_source_t *self, sf_count_t pos);
typedef void (*data_source_narrow_fn_t)(data_source_t *self, sf_count_t start, sf_count_t end);
typedef sf_count_t (*data_source_position_fn_t)(data_source_t *self);

data_source_t *simple_data_source(sf_count_t frames, sf_count_t channels);

struct data_source {
  data_source_read_fn_t read_fn;
  data_source_write_fn_t write_fn;
  data_source_seek_fn_t seek_fn;
  data_source_narrow_fn_t narrow_fn;
  data_source_position_fn_t position_fn;
  data_source_position_fn_t write_position_fn;
  void *data;
};
