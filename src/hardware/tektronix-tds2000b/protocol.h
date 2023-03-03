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

enum TEK_DATA_ENCODING {
	ENC_ASCII,
	ENC_BINARY
};

enum TEK_DATA_FORMAT {
	FMT_RI,
	FMT_RP
};

enum TEK_DATA_ORDERING {
	ORDER_LSB,
	ORDER_MSB
};

enum TEK_POINT_FORMAT {
	PT_FMT_ENV,
	PT_FMT_Y
};

enum TEK_X_UNITS {
	XU_SECOND,
	XU_HZ
};

enum TEK_Y_UNITS {
	YU_UNKNOWN,
	YU_UNKNOWN_MASK,
	YU_VOLTS,
	YU_DECIBELS,

	// TBS1000B/EDU, TBS1000, TDS2000C, TDS1000C-EDU, TDS2000B, 
	// TDS1000B, TPS2000B, and TPS2000 Series only:
	YU_AMPS,
	YU_VV,
	YU_VA,
	YU_AA
};


struct most_recent_wave_preamble {
	//For Y format, the time (absolute coordinate) of a point, relative to the trigger, can
//be calculated using the following formula. N ranges from 0 to 2499.
//X n = XZEro + XINcr (n - PT_OFf)
	// double x_mult;
	// double x_off;
	float x_zero; // (in xunis)
	float x_incr; // seconds per point or herts per point
	enum TEK_X_UNITS x_unit; // s or hz

//value_in_YUNits = ((curve_in_dl - YOFF_in_dl) * YMUlt) + YZERO_in_YUNits

	float y_mult; // (in yunits)
	float y_off; // (in digitizer levels)
	float y_zero; // (in yunits)
	enum TEK_Y_UNITS y_unit; // Volts, U, db, A, VA, AA, VV (semi-conditional)

	int num_pts;

	// char* wfid_description;

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

	struct most_recent_wave_preamble wavepre;

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
