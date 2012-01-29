/**
 * This file is part of rove.
 * rove is copyright 2007-2009 william light <visinin@gmail.com>
 *
 * rove is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rove is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rove.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>

#include <math.h>
#include <sndfile.h>
#include <jack/jack.h>

#ifdef HAVE_SRC
#include <samplerate.h>
#endif

#include "types.h"
#include "group.h"
#include "rmonome.h"
#include "file.h"
#include "ringbuffer.h"
#include "resampler.h"

#define FILE_T(x) ((file_t *) x)

extern state_t state;

void file_change_status(file_t *self, file_status_t nstatus);

static sf_count_t calculate_play_pos(sf_count_t length, int x, int y, uint_t reverse, uint_t rows, uint_t cols) {
	double elapsed;

	x &= 0x0F;

	if( reverse )
		x += 1;

	x += y * cols;
	elapsed = x / (double) (cols * rows);

	if( reverse )
		return lrint(floor(elapsed * length));
	else
		return lrint(ceil(elapsed * length));
}

static void calculate_monome_pos(sf_count_t length, sf_count_t position, uint_t rows, uint_t cols, r_monome_position_t *pos) {
	double elapsed;
	int x, y;

	elapsed = position / (double) length;
	x  = lrint(floor(elapsed * (((double) cols) * rows)));
	y  = (x / cols) & 0x0F;
	x %= cols;

	pos->x = x;
	pos->y = y;
}


static void file_record(file_t *self, jack_default_audio_sample_t **buffers, int channels, jack_nframes_t nframes, jack_nframes_t sample_rate) {
	self->sample_rate = sample_rate;
	self->length = self->file_length = self->loop * state.frames_per_beat;
	self->speed = -1;

//	printf("recording %d frames @ %llu\n", nframes, self->scratch->write_position_fn(self->scratch));

	self->scratch->read_fn(self->scratch, buffers, channels, nframes, sample_rate);

	if (self->scratch->write_position_fn(self->scratch) < 1) {
//	  printf("finished writing @ %llu %llu %llu\n", self->scratch->write_position_fn(self->scratch), self->length, self->file_length);
		file_change_status(self, FILE_STATUS_INACTIVE);
		self->source = self->scratch;
	}
}

static void file_process(file_t *self, jack_default_audio_sample_t **buffers, int channels, jack_default_audio_sample_t **input_buffers, int input_channels, jack_nframes_t nframes, jack_nframes_t sample_rate) {
  if (self->status == FILE_STATUS_RECORDING) {
    file_record(self, input_buffers, input_channels, nframes, sample_rate);
  } else {
	double speed;

	speed = (sample_rate / (double) self->sample_rate) * (1 / self->speed);

        self->source->write_fn(self->source, buffers, channels, nframes, sample_rate, speed, self->volume);
  }
}


static void file_monome_out(file_t *self, r_monome_t *monome) {
	static int blink = 0;
	r_monome_position_t pos;
	uint16_t r = 0;

	/* a uint16_t is the same thing as an array of two uint8_t's, which
	   monome_led_row expects. */
	uint8_t *row = (uint8_t *) &r;

	calculate_monome_pos(
		self->file_length, file_get_play_pos(self),
		self->row_span, (self->columns) ? self->columns : monome->cols, &pos);

	if( MONOME_POS_CMP(&pos, &self->monome_pos_old)
		|| self->force_monome_update
		|| (!file_mapped(self) && !(blink = (blink + 1) % 7)) ) {
		if( self->force_monome_update ) {
			monome->dirty_field &= ~(1 << self->y);
			self->force_monome_update = 0;

			monome_led_set(monome->dev, self->group->idx, 0,
			               !!self->group->active_loop);
		}

		if( pos.y != self->monome_pos_old.y )
			monome_led_row(monome->dev, 0, self->y + self->monome_pos_old.y, 2, row);

		MONOME_POS_CPY(&self->monome_pos_old, &pos);

		if( file_is_active(self) )
			r = 1 << pos.x;

		if( !file_mapped(self) ) {
			if( random() & 1 && file_is_active(self) )
				monome_led_set(monome->dev, self->group - state.groups, 0, 1);
			else {
				monome_led_set(monome->dev, self->group - state.groups, 0, 0);
				r = 0;
			}
		}

		monome_led_row(monome->dev, 0, self->y + pos.y, 2, row);
	}

	MONOME_POS_CPY(&self->monome_pos, &pos);
}

static void file_monome_in(r_monome_t *monome, uint_t x, uint_t y, uint_t type, void *user_arg) {
	file_t *self = FILE_T(user_arg);
	unsigned int cols, x_frame;

	r_monome_position_t pos = {x, y - self->y};

	switch( type ) {
	case MONOME_BUTTON_DOWN:
		if( y < self->y || y > ( self->y + self->row_span - 1) )
			return;

		cols = (self->columns) ? self->columns : monome->cols;
		if( x > cols - 1 )
			return;

                x_frame = calculate_play_pos(self->file_length, pos.x, pos.y,
                                 (self->play_direction == FILE_PLAY_DIRECTION_REVERSE),
                                             self->row_span, cols);
          if (!self->setting_loop) {
            self->loop_start = x_frame;
            self->looping = 0;
            self->setting_loop = 1;

            self->new_offset = x_frame;

            file_on_quantize(self, file_seek);
          } else {
            self->looping = 1;
            self->setting_loop = 0;
            self->loop_end = x_frame;
            self->source->narrow_fn(self->source, self->loop_start, self->loop_end);
            printf("loop from %d to %d (%d)\n", self->loop_start, self->loop_end, self->loop_end - self->loop_start);
          }
          break;
	case MONOME_BUTTON_UP:
          self->setting_loop = 0;
		break;
	}
}

static void file_init(file_t *self) {
	self->status         = FILE_STATUS_INACTIVE;
	self->play_direction = FILE_PLAY_DIRECTION_FORWARD;
	self->volume         = 1.0;

	self->process_cb     = file_process;
	self->monome_out_cb  = file_monome_out;
	self->monome_in_cb   = file_monome_in;

        self->setting_loop = 0;
        self->looping = 0;
        self->loop_start = 0;
        self->loop_end = 0;

	self->scratch = 0;
}

void file_free(file_t *self) {
	free(self->file_data);
	// Need to free source+scratch
	free(self);
}

file_t *file_new_from_path(const char *path) {
	file_t *self;
	SF_INFO info;
	SNDFILE *snd;

	if( !(self = calloc(sizeof(file_t), 1)) )
		return NULL;

	file_init(self);

	if( !(snd = sf_open(path, SFM_READ, &info)) ) {
		printf("file: couldn't load \"%s\".  sorry about your luck.\n%s\n\n", path, sf_strerror(snd));

		free(self);
		return NULL;
	}

	self->length      = self->file_length = info.frames;
	self->channels    = info.channels;
	self->sample_rate = info.samplerate;
	self->file_data   = calloc(sizeof(float), info.frames * info.channels);

	if( sf_readf_float(snd, self->file_data, info.frames) != info.frames ) {
		file_free(self);
		self = NULL;
	}

	self->source = resampler_data_source(self->file_data, info.frames, info.channels);

	if (0 && info.samplerate != 44100) {
	  printf("resampling\n");
          SRC_DATA data;
          data.data_in = self->file_data;
          data.input_frames = info.frames;
          data.src_ratio = 44100.0/info.samplerate;
          data.output_frames = ceil(info.frames * data.src_ratio);
          data.data_out = calloc(sizeof(float), data.output_frames * info.channels);
          src_simple(&data, SRC_SINC_MEDIUM_QUALITY, info.channels);

//          self->timestretcher = timestretcher_init(data.data_out, data.output_frames, info.channels, 44100);
          self->length = self->file_length = data.output_frames;
          self->sample_rate = 44100;
          free(data.data_out);
	} else {
//          self->timestretcher = timestretcher_init(self->file_data, info.frames, info.channels, 44100);
	}

	sf_close(snd);

//        printf("Initialising rubberband with %lld %lld\n", self->sample_rate, self->length);

//        rubberband_set_default_debug_level(1);

//        self->rbState = rubberband_new(self->sample_rate, self->channels, RubberBandOptionProcessRealTime, 1.0, 1.0);
//        self->rbState = rubberband_new(self->sample_rate, self->channels, 0, 1.0, 1.0);
//        rubberband_set_max_process_size(self->rbState, self->length);

//        rubberband_study(self->rbState, (const float *const *)rb_data, self->length, 1);
//        rubberband_process(self->rbState, (const float *const*)rb_data, self->length, 1);

	return self;
}

void file_set_play_pos(file_t *self, sf_count_t p) {
	if( p >= self->file_length )
		p %= self->file_length;

	if( p < 0 )
		p = self->file_length - (abs(p) % self->file_length);

        if (self->looping &&
            (((self->loop_start < self->loop_end) && p > self->loop_end) ||
             (p > self->loop_end && p < self->loop_start)))
          {
          self->play_offset = self->loop_start;
        } else
          self->play_offset = p;

//        printf("setting play_pos to %llu, recording? %d\n", self->play_offset, state.recording);
	self->source->seek_fn(self->source, self->play_offset);
}

void file_inc_play_pos(file_t *self, sf_count_t delta) {
	if( self->play_direction == FILE_PLAY_DIRECTION_REVERSE )
		file_set_play_pos(self, self->play_offset - delta);
	else
		file_set_play_pos(self, self->play_offset + delta);
}

sf_count_t file_get_play_pos(file_t *self) {
	sf_count_t pos;
	if (self->status == FILE_STATUS_ACTIVE) {
                pos = self->source->position_fn(self->source);
	} else {
                pos = self->scratch->write_position_fn(self->source);
	}

	return pos;
}

void file_change_status(file_t *self, file_status_t nstatus) {
	switch(self->status) {
	case FILE_STATUS_ACTIVE:
	case FILE_STATUS_RECORDING:
		switch(nstatus) {
		case FILE_STATUS_ACTIVE:
                case FILE_STATUS_RECORDING:
			return;

		case FILE_STATUS_INACTIVE:
			if( self->group->active_loop == self )
				self->group->active_loop = NULL;

			break;
		}
		break;

	case FILE_STATUS_INACTIVE:
		switch(nstatus) {
		case FILE_STATUS_ACTIVE:
                case FILE_STATUS_RECORDING:
			group_activate_file(self);
			break;

		case FILE_STATUS_INACTIVE:
			return;
		}
		break;
	}

	self->status = nstatus;
	file_force_monome_update(self);
}

void file_deactivate(file_t *self) {
	file_change_status(self, FILE_STATUS_INACTIVE);

	/* XXX: HACK */
	if( !file_mapped(self) )
		file_monome_out(self, self->mapped_monome);
}

void file_seek(file_t *self) {
	file_change_status(self, state.recording ? FILE_STATUS_RECORDING : FILE_STATUS_ACTIVE);
	file_set_play_pos(self, self->new_offset);

	self->source->narrow_fn(self->source, 0, self->length);

}

void file_on_quantize(file_t *self, quantize_callback_t cb) {
	if( cb )
		self->mapped_monome->quantize_field |= 1 << self->y;
	else
		self->mapped_monome->quantize_field &= ~(1 << self->y);

	self->quantize_cb = cb;
}

void file_force_monome_update(file_t *self) {
	self->force_monome_update = 1;
	self->mapped_monome->dirty_field |= 1 << self->y;
}
