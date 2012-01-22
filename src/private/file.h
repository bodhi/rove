/**
 * This file is part of rove.
 * rove is copyright 2007, 2008 william light <visinin@gmail.com>
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

#ifndef _ROVE_FILE_H
#define _ROVE_FILE_H

#include <sndfile.h>
#include <stdint.h>

#include "types.h"

#define file_mapped(x) (x->mapped_monome->callbacks[x->y].data == x)
#define file_is_active(f) (f->status != FILE_STATUS_INACTIVE)
//#define file_get_play_pos(f) (f->play_offset * f->channels)

file_t *file_new_from_path(const char *path);
void file_free(file_t *self);

void file_set_play_pos(file_t *self, sf_count_t pos);
void file_inc_play_pos(file_t *self, sf_count_t delta);
sf_count_t file_get_play_pos(file_t *self);

void file_deactivate(file_t *self);
void file_seek(file_t *self);

void file_on_quantize(file_t *self, quantize_callback_t cb);
void file_force_monome_update(file_t *self);

#endif
