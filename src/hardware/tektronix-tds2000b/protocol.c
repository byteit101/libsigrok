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

#include <config.h>
#include <errno.h>
#include <glib.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "scpi.h"
#include "protocol.h"


struct tek_enum_parser {
	int enum_value;
	const char* name;
};

static const struct tek_enum_parser parse_table_data_encoding[] = {
	{ ENC_ASCII, "ASC" },
	{ ENC_BINARY, "BIN" },
	{ 0, NULL}
};
static const struct tek_enum_parser parse_table_data_format[] = {
	{ FMT_RI, "RI" },
	{ FMT_RP, "RP" },
	{ 0, NULL}
};
static const struct tek_enum_parser parse_table_data_ordering[] = {
	{ ORDER_LSB, "LSB" },
	{ ORDER_MSB, "MSB" },
	{ 0, NULL}
};
static const struct tek_enum_parser parse_table_point_format[] = {
	{ PT_FMT_ENV, "ENV" },
	{ PT_FMT_Y, "Y" },
	{ 0, NULL}
};
static const struct tek_enum_parser parse_table_xunits[] = {
	{ XU_SECOND, "s" },
	{ XU_HZ, "Hz" },
	{ 0, NULL}
};
static const struct tek_enum_parser parse_table_yunits[] = {
	{ YU_UNKNOWN, "U" },
	{ YU_UNKNOWN_MASK, "?" },
	{ YU_VOLTS, "Volts" },
	{ YU_DECIBELS, "dB" },

	// select models only:
	{ YU_AMPS, "A" },
	{ YU_AA, "AA" },
	{ YU_VA, "VA" },
	{ YU_VV, "VV" },
	{ 0, NULL}
};

static int tek_tds2000b_read_header(struct sr_dev_inst *sdi);

SR_PRIV int tek_tds2000b_receive(int fd, int revents, void *cb_data)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;

	struct sr_scpi_dev_inst *scpi;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	struct sr_channel *ch;
	int len, i;
	float wait;

	(void)fd;

	sdi = cb_data;
	if (!sdi)
		return TRUE;

	devc = sdi->priv;
	if (!devc)
		return TRUE;
	scpi = sdi->conn;
	ch = devc->channel_entry->data;

		sr_err("Doing receive %d.", revents);

	if (revents == G_IO_IN || TRUE) { // this is always 0 for some reason
		/* TODO */
	
		sr_err("Doing receive Good.");



//	if (devc->num_block_bytes == 0) {
		/* Wait for the device to fill its output buffers. */
		/* The older models need more time to prepare the the output buffers due to CPU speed. */
		wait = (2500 * 2.5);
		sr_dbg("Waiting %.f0 ms for device to prepare the output buffers", wait / 1000);
		g_usleep(wait);
		if (sr_scpi_read_begin(scpi) != SR_OK)
			return TRUE;
		

		sr_dbg("New block with header expected.");
		len = tek_tds2000b_read_header(sdi);
		if (len == 0)
			/* Still reading the header. */
			return TRUE;
		if (len == -1) {
			sr_err("Read error, aborting capture.");
			std_session_send_df_frame_end(sdi);
			sdi->driver->dev_acquisition_stop(sdi);
			return TRUE;
		}
		//devc->num_block_bytes = len;
		devc->num_block_read = 0;


		if (len == -1) {
			sr_err("Read error, aborting capture.");
			std_session_send_df_frame_end(sdi);
			sdi->driver->dev_acquisition_stop(sdi);
			return TRUE;
		}
		//

		// sr_dbg("Requesting: %" PRIu64 " bytes.", devc->num_samples - devc->num_block_bytes);
		sr_dbg("Requesting: %d bytes.", TEK_BUFFER_SIZE + 1+1+1+4);
		len = sr_scpi_read_data(scpi, (char *)devc->buffer, 6);
		if (len == -1) {
			sr_err("Read error, aborting capture.");
			std_session_send_df_frame_end(sdi);
			sdi->driver->dev_acquisition_stop(sdi);
			return TRUE;
		}
		sr_dbg("Requesting balance: %d bytes.", TEK_BUFFER_SIZE + 1);
		len = sr_scpi_read_data(scpi, (char *)devc->buffer, TEK_BUFFER_SIZE + 1);
		if (len == -1) {
			sr_err("Read error, aborting capture.");
			std_session_send_df_frame_end(sdi);
			sdi->driver->dev_acquisition_stop(sdi);
			return TRUE;
		}
		devc->num_block_read = len;

			while (devc->num_block_read < TEK_BUFFER_SIZE + 1) {
				/* We received all data as one block. */
				/* Offset the data block buffer past the IEEE header and description header. */
				// devc->buffer += devc->block_header_size;
			//} else {
				sr_dbg("Requesting: %d bytes.", TEK_BUFFER_SIZE + 1 - devc->num_block_read);
				len = sr_scpi_read_data(scpi, (char *)devc->buffer + devc->num_block_read, TEK_BUFFER_SIZE + 1 - devc->num_block_read);
				if (len == -1) {
					sr_err("Read error, aborting capture.");
					std_session_send_df_frame_end(sdi);
					sdi->driver->dev_acquisition_stop(sdi);
					return TRUE;
				}
			sr_dbg("Received block: %d bytes.", len);
				devc->num_block_read += len;
				// if (len == 2501 - devc->num_block_read)
				// 	len--; //skip new line
				// devc->num_block_bytes += len;
			}
				len = TEK_BUFFER_SIZE;

				float vdiv = devc->vdiv[ch->index];
				GArray *float_data;
				static GArray *data;
				float voltage, vdivlog;
				int digits;

				data = g_array_sized_new(FALSE, FALSE, sizeof(uint8_t), len);
				g_array_append_vals(data, devc->buffer, len);
				float_data = g_array_new(FALSE, FALSE, sizeof(float));
				for (i = 0; i < len; i++) {
					voltage = (float)g_array_index(data, int8_t, i) - devc->wavepre.y_off;
					voltage = ((devc->wavepre.y_mult * voltage) + devc->wavepre.y_zero);
					g_array_append_val(float_data, voltage);
				}
				vdivlog = log10f(vdiv);
				digits = -(int) vdivlog + (vdivlog < 0.0);
				sr_analog_init(&analog, &encoding, &meaning, &spec, digits);
				analog.meaning->channels = g_slist_append(NULL, ch);
				analog.num_samples = float_data->len;
				analog.data = (float *)float_data->data;
				analog.meaning->mq = SR_MQ_VOLTAGE;
				analog.meaning->unit = SR_UNIT_VOLT;
				analog.meaning->mqflags = 0;
				packet.type = SR_DF_ANALOG;
				packet.payload = &analog;
				sr_session_send(sdi, &packet);
				g_slist_free(analog.meaning->channels);
				g_array_free(data, TRUE);
			len = 0;
			if (devc->num_block_read >= 2500) {
				sr_dbg("Transfer has been completed.");
			//	devc->num_header_bytes = 0;
			//	devc->num_block_bytes = 0;
				if (!sr_scpi_read_complete(scpi)) {
					sr_err("Read should have been completed.");
					std_session_send_df_frame_end(sdi);
					sdi->driver->dev_acquisition_stop(sdi);
					return TRUE;
				}
				devc->num_block_read = 0;
			} else {
				sr_dbg("%d of %d block bytes read.",
					devc->num_block_read, 2501);
			}

		if (devc->channel_entry->next) {
				sr_dbg("Doing another channel");
			/* We got the frame for this channel, now get the next channel. */
			devc->channel_entry = devc->channel_entry->next;
			tek_tds2000b_channel_start(sdi);
		} else {
			/* Done with this frame. */
			std_session_send_df_frame_end(sdi);
			if (++devc->num_frames == devc->limit_frames) {
				/* Last frame, stop capture. */
				sdi->driver->dev_acquisition_stop(sdi);
			} else {
				sr_dbg("Doing another frame");
				/* Get the next frame, starting with the first channel. */
				devc->channel_entry = devc->enabled_channels;
				tek_tds2000b_capture_start(sdi);

				/* Start of next frame. */
				std_session_send_df_frame_begin(sdi);
			}
		}
	}
	return TRUE;
}


/* Start reading data from the current channel. */
SR_PRIV int tek_tds2000b_channel_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_channel *ch;

	if (!(devc = sdi->priv))
		return SR_ERR;

	ch = devc->channel_entry->data;

	sr_dbg("Start reading data from channel %s.", ch->name);

	if (sr_scpi_send(sdi->conn, "DAT:SOU CH%d",
		ch->index + 1) != SR_OK)
			return SR_ERR;

	sr_dbg("Starting wfm");
	if (sr_scpi_send(sdi->conn, "WAVF?") != SR_OK)
			return SR_ERR;

	sr_dbg("trigger wfm");

	devc->num_block_read = 0;

	return SR_OK;
}

/* Start capturing a new frameset. */
SR_PRIV int tek_tds2000b_capture_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	if (!(devc = sdi->priv))
		return SR_ERR;

		// TODO: one shot vs stopped vs continuous vs ... mode

	/**
	 * 
	 *	dat:enc = bin...?
	 *	dat:wid = 1
	 * run
	 * 
	 * for each channel
	 * {
	 * 		dat:sou = CH<x>
	 * 		*wai / *opp?
	 *      CURV?
	 * 		WFMPre?
	 * }
	*/
	unsigned int framecount;
	

	if (tek_tds2000b_channel_start(sdi) != SR_OK)
		return SR_ERR;

	sr_dbg("Starting data capture for curves.");
	// if (tek_tds2000b_config_set(sdi, "WAVF?") != SR_OK)
	// 	return SR_ERR;
	
	// ret = sr_scpi_read_data(sdi->conn, buf, 200);
	// if (ret < 0) {
	// 	sr_err("Read error while reading data header.");
	// 	return SR_ERR;
	// }
	// tek_tds2000b_set_wait_event(devc, WAIT_STOP);

	return SR_OK;
}

static int parse_scpi_int(const char *data, int *out_err, int default_value)
{
	int value = default_value;
	struct sr_rational ret_rational;
	int ret;

	ret = sr_parse_rational(data, &ret_rational);
	if (ret == SR_OK && (ret_rational.p % ret_rational.q) == 0) {
		value = ret_rational.p / ret_rational.q;
	} else {
		sr_dbg("get_int: non-integer rational=%" PRId64 "/%" PRIu64,
			ret_rational.p, ret_rational.q);
		*out_err = SR_ERR_DATA;
	}

	return value;
}

static float parse_scpi_float(const char *data, int *out_err, float default_value)
{
	float value = default_value;

	if (sr_atof_ascii(data, &value) != SR_OK)
		*out_err = SR_ERR_DATA;

	return value;
}


static const char* parse_scpi_string(char *data, int *out_err)
{
	(void)out_err;
	return sr_scpi_unquote_string(data);
}

static int parse_scpi_enum(const char* data, const struct tek_enum_parser* parser_table, int *out_err, int default_value)
{
	while (parser_table->name)
	{
		// TODO: check if really case insensitive
		if (g_ascii_strcasecmp(parser_table->name, data) == 0)
		{
			return parser_table->enum_value;
		}
		parser_table++;
	}
	*out_err = SR_ERR_DATA;
	return default_value;
}

static const char* render_scpi_enum(int value, const struct tek_enum_parser* parser_table, int *out_err)
{
	while (parser_table->name)
	{
		if (value == parser_table->enum_value)
		{
			return parser_table->name;
		}
		parser_table++;
	}
	*out_err = SR_ERR_DATA;
	return "NULL";
}

static int tek_tds2000b_parse_header(struct sr_dev_inst *sdi, char* end_buf)
{
	struct sr_scpi_dev_inst *scpi = sdi->conn;
	struct dev_context *devc = sdi->priv;
	char *buf = (char *)devc->buffer;
	char *fields[17]; // one extra for block ()
	int i = 0;
	int ret = SR_OK;

	sr_dbg("Parsing buffer of size %d", (int)(end_buf - buf));
	sr_dbg("Line as recved as: %.*s", (int)(end_buf - buf - 1), buf);

	// Parse in 3 steps:
	// 1. find all semicolons, and replace with null bytes
	// 2. Put next char reference into array
	// 3. Parse each type based on array index
	fields[i++] = buf;
	while (buf < end_buf)
	{
		if (*buf == ';')
		{
			*buf = 0; // turn into list of C strings
			fields[i++] = buf+1;
		}
		buf++;
	}
	sr_dbg("Expected 17 indexes, found %d in header", i);
/*
BYT_Nr <NR1>;
BIT_Nr <NR1>;
ENCdg { ASC | BIN };
BN_Fmt { RI | RP };
BYT_Or { LSB | MSB };
NR_Pt <NR1>;
WFID <Qstring>;
PT_FMT {ENV | Y};
XINcr <NR3>;
PT_Off <NR1>;
XZERo <NR3>;
XUNit<QString>;
YMUlt <NR3>;
YZEro <NR3>;
YOFF <NR3>;
YUNit <QString>;
#..block
*/
	i = 0;
	// TODO: assert a few of these values
	int byte_width = parse_scpi_int(fields[i++], &ret, 1);
	int bit_width = parse_scpi_int(fields[i++], &ret, 8);
	enum TEK_DATA_ENCODING encoding = parse_scpi_enum(fields[i++], parse_table_data_encoding, &ret, ENC_ASCII);
	enum TEK_DATA_FORMAT format = parse_scpi_enum(fields[i++], parse_table_data_format, &ret, FMT_RI);
	enum TEK_DATA_ORDERING ordering = parse_scpi_enum(fields[i++], parse_table_data_ordering, &ret, ORDER_LSB);
	devc->wavepre.num_pts = parse_scpi_int(fields[i++], &ret, -1);
	const char* wfid = parse_scpi_string(fields[i++], &ret);
	enum TEK_POINT_FORMAT pt_format = parse_scpi_enum(fields[i++], parse_table_point_format, &ret, PT_FMT_Y);
	devc->wavepre.x_incr = parse_scpi_float(fields[i++], &ret, 1);
	int pt_off = parse_scpi_int(fields[i++], &ret, 0); // always zero, parsed only for completeness
	devc->wavepre.x_zero = parse_scpi_float(fields[i++], &ret, 0);
	devc->wavepre.x_unit = parse_scpi_enum(sr_scpi_unquote_string(fields[i++]), parse_table_xunits, &ret, XU_SECOND);
	devc->wavepre.y_mult = parse_scpi_float(fields[i++], &ret, 0);
	devc->wavepre.y_zero = parse_scpi_float(fields[i++], &ret, 0);
	devc->wavepre.y_off = parse_scpi_float(fields[i++], &ret, 0);
	devc->wavepre.y_unit = parse_scpi_enum(sr_scpi_unquote_string(fields[i++]), parse_table_yunits, &ret, YU_UNKNOWN);
	//int blocklength = parse_scpi_blockstart(fields[i++], &ret);
	sr_dbg("Expected 17 indexes, parsed %d in header with %i", i, ret);

	sr_dbg("Line is parsed as: %d;%d;%s;%s;%s;%i;\"%s\";%s;%.2e;%i;%.2e;\"%s\";%.2e;%.2e;%.2e;\"%s\"; ",
		byte_width, bit_width, 
		render_scpi_enum(encoding, parse_table_data_encoding, &ret),
		render_scpi_enum(format, parse_table_data_format, &ret),
		render_scpi_enum(ordering, parse_table_data_ordering, &ret),
		devc->wavepre.num_pts,
		wfid,
		render_scpi_enum(pt_format, parse_table_point_format, &ret),
		devc->wavepre.x_incr, pt_off, devc->wavepre.x_zero,
		render_scpi_enum(devc->wavepre.x_unit, parse_table_xunits, &ret),
		devc->wavepre.y_mult,devc->wavepre.y_zero, devc->wavepre.y_off,
		render_scpi_enum(devc->wavepre.y_unit, parse_table_yunits, &ret)
	);
	return ret;
}

/* Read the header of a data block. */
static int tek_tds2000b_read_header(struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi = sdi->conn;
	struct dev_context *devc = sdi->priv;
	char *buf = (char *)devc->buffer;
	int ret;

	// header is variable, but at least 100 bytes, and likely no more than 175 bytes



// 16 args, ignore all for now TODO: !!
int attempt = 100;
int found = 0;
	while (found < 16) 

	{

	sr_dbg("Device searching %i semis.", found);
	/* Read header from device. */
	ret = sr_scpi_read_data(scpi, buf, attempt);
	if (ret < attempt) {
		sr_err("Read error while reading data header. true: %i of %i", ret, attempt);
		return SR_ERR;
	}
	sr_dbg("Device searching et = %d .", ret);
	for (int i = 0; i < ret; i++, buf++){
		if (*buf == ';')
		{
			found++;
		}
	}
	attempt = 16 - found;
	if (attempt > 1)
		attempt *=2;
}
	sr_dbg("Device returned %i bytes.", ret);
	//ret = 
	if (tek_tds2000b_parse_header(sdi, buf+1) != SR_OK)
		ret = -1;
	// devc->num_header_bytes += ret;
	// buf += block_offset; /* Skip to start descriptor block. */

	// /* Parse WaveDescriptor header. */
	// memcpy(&desc_length, buf + 36, 4); /* Descriptor block length */
	// memcpy(&data_length, buf + 60, 4); /* Data block length */

	// devc->block_header_size = desc_length + 15;
	// devc->num_samples = data_length;

	// sr_dbg("Received data block header: -> block length %d.", ret);

	return ret;
}
/* Send a configuration setting. */
SR_PRIV int tek_tds2000b_config_set(const struct sr_dev_inst *sdi, const char *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);
	ret = sr_scpi_send_variadic(sdi->conn, format, args);
	va_end(args);

	return ret;
}

SR_PRIV int tek_tds2000b_get_dev_cfg_vertical(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	char *cmd;
	int i;
	int res;

	devc = sdi->priv;

	/* Vertical gain. */
	for (i = 0; i < devc->model->channels; i++) {
		cmd = g_strdup_printf("CH%d:SCA?", i + 1);
		res = sr_scpi_get_float(sdi->conn, cmd, &devc->vdiv[i]);
		g_free(cmd);
		if (res != SR_OK)
			return SR_ERR;
	}
	sr_dbg("Current vertical gain:");
	for (i = 0; i < devc->model->channels; i++)
		sr_dbg("CH%d %g", i + 1, devc->vdiv[i]);

	/* Vertical offset. */
	for (i = 0; i < devc->model->channels; i++) {
		cmd = g_strdup_printf("CH%d:POS?", i + 1);
		res = sr_scpi_get_float(sdi->conn, cmd, &devc->vert_offset[i]);
		g_free(cmd);
		if (res != SR_OK)
			return SR_ERR;
	}
	sr_dbg("Current vertical offset:");
	for (i = 0; i < devc->model->channels; i++)
		sr_dbg("CH%d %g", i + 1, devc->vert_offset[i]);

	return SR_OK;
}


SR_PRIV int tek_tds2000b_get_dev_cfg(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_channel *ch;
	char *cmd, *response;
	int i;
	int res;

	devc = sdi->priv;

	/* Analog channel state. */
	for (i = 0; i < devc->model->channels; i++) {
		cmd = g_strdup_printf("SELECT:CH%i?", i + 1);
		res = sr_scpi_get_bool(sdi->conn, cmd, &devc->analog_channels[i]);
		g_free(cmd);
		if (res != SR_OK)
			return SR_ERR;
		ch = g_slist_nth_data(sdi->channels, i);
		ch->enabled = devc->analog_channels[i];
	}
	sr_dbg("Current analog channel state:");
	for (i = 0; i < devc->model->channels; i++)
		sr_dbg("CH%d %s", i + 1, devc->analog_channels[i] ? "On" : "Off");


	/* Timebase. */
	if (sr_scpi_get_float(sdi->conn, "hor:del:sca?", &devc->timebase) != SR_OK)
		return SR_ERR;
	sr_dbg("Current timebase: %g.", devc->timebase);

	/* Probe attenuation. */
	for (i = 0; i < devc->model->channels; i++) {
		cmd = g_strdup_printf("CH%d:PROBE?", i + 1);
		res = sr_scpi_get_float(sdi->conn, cmd, &devc->attenuation[i]);
		g_free(cmd);
		if (res != SR_OK)
			return SR_ERR;
	}
	sr_dbg("Current probe attenuation:");
	for (i = 0; i < devc->model->channels; i++)
		sr_dbg("CH%d %g", i + 1, devc->attenuation[i]);

	/* Vertical gain and offset. */
	if (tek_tds2000b_get_dev_cfg_vertical(sdi) != SR_OK)
		return SR_ERR;


	if (tek_tds2000b_get_dev_cfg_horizontal(sdi) != SR_OK)
		return SR_ERR;

	/* Coupling. */
	for (i = 0; i < devc->model->channels; i++) {
		cmd = g_strdup_printf("CH%d:COUP?", i + 1);
		g_free(devc->coupling[i]);
		devc->coupling[i] = NULL;
		res = sr_scpi_get_string(sdi->conn, cmd, &devc->coupling[i]);
		g_free(cmd);
		if (res != SR_OK)
			return SR_ERR;
	}

	sr_dbg("Current coupling:");
	for (i = 0; i < devc->model->channels; i++)
		sr_dbg("CH%d %s", i + 1, devc->coupling[i]);

	/* Trigger source. edge, pulse, and video are always the same, it appears */
	response = NULL;
	g_free(devc->trigger_source);
	if (sr_scpi_get_string(sdi->conn, "TRIG:MAI:edge:sou?", &devc->trigger_source) != SR_OK)
		return SR_ERR;
	sr_dbg("Current trigger source: %s.", devc->trigger_source);

	/* Horizontal trigger position. */
	if (sr_scpi_get_float(sdi->conn, "hor:pos?", &devc->horiz_triggerpos) != SR_OK)
		return SR_ERR;
	
	// triggerpos is in timeunits, convert back to percentage
	devc->horiz_triggerpos = (-devc->horiz_triggerpos / (devc->timebase * 10)) + 0.5;
		// t_dbl = -(devc->horiz_triggerpos - 0.5) * devc->timebase * devc->num_timebases;

	sr_dbg("Current horizontal trigger position %.10f.", devc->horiz_triggerpos);

	/* Trigger slope. */
	g_free(devc->trigger_slope);
	devc->trigger_slope = NULL;
	res = sr_scpi_get_string(sdi->conn, "trig:mai:edge:slope?", &devc->trigger_slope);
	if (res != SR_OK)
		return SR_ERR;
	sr_dbg("Current trigger slope: %s.", devc->trigger_slope);

	/* Trigger level. */
	res = sr_scpi_get_float(sdi->conn, "trig:mai:lev?", &devc->trigger_level);
	if (res != SR_OK)
		return SR_ERR;
	sr_dbg("Current trigger level: %g.", devc->trigger_level);


	/* Averaging */
	response = NULL;
	if (sr_scpi_get_string(sdi->conn, "acq:mod?", &response) != SR_OK)
		return SR_ERR;
	devc->average_enabled = g_ascii_strncasecmp(response, "average", 3) == 0;
	sr_dbg("Acquisition mode: %s.", response);
	g_free(response);

	if (sr_scpi_get_int(sdi->conn, "acq:numav?", &devc->average_samples) != SR_OK)
		return SR_ERR;
	sr_dbg("Averaging samples: %i.", devc->average_samples);


	return SR_OK;
}


SR_PRIV int tek_tds2000b_get_dev_cfg_horizontal(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	float fvalue;

	devc = sdi->priv;

	// all scopes have -5 to +5 hdivs
	// and -4 to +4 vdivs


	/* Get the timebase. */
	if (sr_scpi_get_float(sdi->conn, "hor:sca?", &devc->timebase) != SR_OK)
		return SR_ERR;

	int memory_depth;

	if (sr_scpi_get_int(sdi->conn, "hor:reco?", &memory_depth) != SR_OK)
		return SR_ERR;

	// TODO: check memory_depth == 2500	

	sr_dbg("Current timebase: %g.", devc->timebase);
	fvalue = TEK_BUFFER_SIZE  / (devc->timebase * (float)TEK_NUM_HDIV);
	devc->samplerate = MIN(fvalue, devc->model->sample_rate * 1000000.0);
	if (devc->samplerate < fvalue)
		sr_dbg("Current samplerate: %0g (limited by device).", devc->samplerate);
	else
		sr_dbg("Current samplerate: %0g.", devc->samplerate);
	sr_dbg("Current memory depth: %d.", TEK_BUFFER_SIZE);// TODO: peak detect mode is half of this

	return SR_OK;
}
