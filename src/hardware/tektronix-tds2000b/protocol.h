/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2023 Patrick Plenefisch <simonpatp@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_HARDWARE_TEKTRONIX_TDS2000B_PROTOCOL_H
#define LIBSIGROK_HARDWARE_TEKTRONIX_TDS2000B_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "tektronix-tds2000b"


struct mini_device_spec {
	const char *model;
	int channels;
};

#define MAX_ANALOG_CHANNELS 4
struct dev_context {
	struct sr_channel_group **analog_groups;
	struct mini_device_spec *model;
	gboolean analog_channels[MAX_ANALOG_CHANNELS];
	float timebase;
	float vdiv[MAX_ANALOG_CHANNELS];
	float vert_offset[MAX_ANALOG_CHANNELS];
	float attenuation[MAX_ANALOG_CHANNELS];
	char *coupling[MAX_ANALOG_CHANNELS];
	char *trigger_source;

	uint64_t limit_frames;
	/* Acquisition settings */
	GSList *enabled_channels;
	/* GSList entry for the current channel. */
	GSList *channel_entry;
	/* Number of frames received in total. */
	uint64_t num_frames;

	/* Acq buffers used for reading from the scope and sending data to app. */
	unsigned char *buffer;
	int num_block_read;
};

SR_PRIV int tek_tds2000b_config_set(const struct sr_dev_inst *sdi,
	const char *format, ...);
SR_PRIV int tek_tds2000b_capture_start(const struct sr_dev_inst *sdi);
SR_PRIV int tek_tds2000b_channel_start(const struct sr_dev_inst *sdi);
SR_PRIV int tek_tds2000b_receive(int fd, int revents, void *cb_data);
SR_PRIV int tek_tds2000b_get_dev_cfg(const struct sr_dev_inst *sdi);
SR_PRIV int tek_tds2000b_get_dev_cfg_vertical(const struct sr_dev_inst *sdi);
SR_PRIV int tek_tds2000b_get_dev_cfg_horizontal(const struct sr_dev_inst *sdi);


#endif
