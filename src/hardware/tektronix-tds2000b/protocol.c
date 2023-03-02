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

static int tek_tds2000b_read_header(struct sr_dev_inst *sdi);

SR_PRIV int tek_tds2000b_receive(int fd, int revents, void *cb_data)
{
	const struct sr_dev_inst *sdi;
	struct dev_context *devc;

	struct sr_scpi_dev_inst *scpi;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	struct sr_datafeed_logic logic;
	struct sr_channel *ch;
	int len, i;
	float wait;
	gboolean read_complete = FALSE;

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
		sr_dbg("Requesting: %" PRIu64 " bytes.", 2500+1+1+1+4);
		len = sr_scpi_read_data(scpi, (char *)devc->buffer, 6);
		if (len == -1) {
			sr_err("Read error, aborting capture.");
			std_session_send_df_frame_end(sdi);
			sdi->driver->dev_acquisition_stop(sdi);
			return TRUE;
		}
		sr_dbg("Requesting balance: %" PRIu64 " bytes.", 2500+1);
		len = sr_scpi_read_data(scpi, (char *)devc->buffer, 2501);
		if (len == -1) {
			sr_err("Read error, aborting capture.");
			std_session_send_df_frame_end(sdi);
			sdi->driver->dev_acquisition_stop(sdi);
			return TRUE;
		}
		devc->num_block_read = len;

			while (devc->num_block_read < 2501) {
				/* We received all data as one block. */
				/* Offset the data block buffer past the IEEE header and description header. */
				// devc->buffer += devc->block_header_size;
			//} else {
				sr_dbg("Requesting: %" PRIu64 " bytes.", 2501 - devc->num_block_read);
				len = sr_scpi_read_data(scpi, (char *)devc->buffer + devc->num_block_read, 2501 - devc->num_block_read);
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
				len = 2500;

				float vdiv = devc->vdiv[ch->index];
				float offset = devc->vert_offset[ch->index];
				GArray *float_data;
				static GArray *data;
				float voltage, vdivlog;
				int digits;

				data = g_array_sized_new(FALSE, FALSE, sizeof(uint8_t), len);
				g_array_append_vals(data, devc->buffer, len);
				float_data = g_array_new(FALSE, FALSE, sizeof(float));
				for (i = 0; i < len; i++) {
					voltage = (float)g_array_index(data, int8_t, i) / 8;
					voltage = ((vdiv * voltage) - offset);
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
				read_complete = TRUE;
				if (!sr_scpi_read_complete(scpi)) {
					sr_err("Read should have been completed.");
					std_session_send_df_frame_end(sdi);
					sdi->driver->dev_acquisition_stop(sdi);
					return TRUE;
				}
				devc->num_block_read = 0;
			} else {
				sr_dbg("%" PRIu64 " of %" PRIu64 " block bytes read.",
					devc->num_block_read, 2501);
			}

		if (devc->channel_entry->next) {
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
	const char *s;

	if (!(devc = sdi->priv))
		return SR_ERR;

	ch = devc->channel_entry->data;

	sr_dbg("Start reading data from channel %s.", ch->name);

	if (sr_scpi_send(sdi->conn, "DAT:SOU CH%d",
		ch->index + 1) != SR_OK)
			return SR_ERR;

	sr_dbg("Starting wfm");
	//	if (sr_scpi_read_begin(sdi->conn) != SR_OK)
	if (sr_scpi_send(sdi->conn, "WAVF?") != SR_OK)
			return SR_ERR;

	sr_dbg("trigger wfm");

	// devc->num_channel_bytes = 0;
	// devc->num_header_bytes = 0;
	// devc->num_block_bytes = 0;

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
	 * 		*wai/*opp?
	 *      CURV?
	 * 		WFMPre?
	 * }
	*/
	unsigned int framecount;
	char buf[200];
	int ret;

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

/* Read the header of a data block. */
static int tek_tds2000b_read_header(struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi = sdi->conn;
	struct dev_context *devc = sdi->priv;
	char *buf = (char *)devc->buffer;
	int ret, desc_length;
	long data_length = 0;

	// header is variable, but at least 100 bytes, and likely no more than 175 bytes

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

// 16 args, ignore all for now TODO: !!
int attempt = 100;
int found = 0;
	while (found < 16) 

	{

	//sr_dbg("Device searching %i semis.", found);
	/* Read header from device. */
	ret = sr_scpi_read_data(scpi, buf, attempt);
	if (ret < attempt) {
		sr_err("Read error while reading data header. true");
		return SR_ERR;
	}
	//sr_dbg("Device searching et = %d .", ret);
	for (int i = 0; i < ret; i++, buf++){
		if (*buf == ';')
			found++;
	}
	attempt = 16 - found;
	if (attempt > 1)
		attempt *=2;
}
	sr_dbg("Device returned %i bytes.", ret);
	// devc->num_header_bytes += ret;
	// buf += block_offset; /* Skip to start descriptor block. */

	// /* Parse WaveDescriptor header. */
	// memcpy(&desc_length, buf + 36, 4); /* Descriptor block length */
	// memcpy(&data_length, buf + 60, 4); /* Data block length */

	// devc->block_header_size = desc_length + 15;
	// devc->num_samples = data_length;

	// sr_dbg("Received data block header: '%s' -> block length %d.", buf, ret);

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
	unsigned int i;
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
	unsigned int i;
	int res, num_tokens;
	gchar **tokens;
	int len;
	float trigger_pos;

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
	tokens = NULL;
	g_free(devc->trigger_source);
	if (sr_scpi_get_string(sdi->conn, "TRIG:MAI:TYP?", &devc->trigger_source) != SR_OK)
		return SR_ERR;
	sr_dbg("Current trigger source: %s.", devc->trigger_source);

	// /* TODO: Horizontal trigger position. */
	// response = "";
	// trigger_pos = 0;
	// // if (sr_scpi_get_string(sdi->conn, g_strdup_printf("%s:TRDL?", devc->trigger_source), &response) != SR_OK)
	// // 	return SR_ERR;
	// // len = strlen(response);
	// len = strlen(tokens[4]);
	// if (!g_ascii_strcasecmp(tokens[4] + (len - 2), "us")) {
	// 	trigger_pos = atof(tokens[4]) / SR_GHZ(1);
	// 	sr_dbg("Current trigger position us %s.", tokens[4] );
	// } else if (!g_ascii_strcasecmp(tokens[4] + (len - 2), "ns")) {
	// 	trigger_pos = atof(tokens[4]) / SR_MHZ(1);
	// 	sr_dbg("Current trigger position ms %s.", tokens[4] );
	// } else if (!g_ascii_strcasecmp(tokens[4] + (len - 2), "ms")) {
	// 	trigger_pos = atof(tokens[4]) / SR_KHZ(1);
	// 	sr_dbg("Current trigger position ns %s.", tokens[4] );
	// } else if (!g_ascii_strcasecmp(tokens[4] + (len - 2), "s")) {
	// 	trigger_pos = atof(tokens[4]);
	// 	sr_dbg("Current trigger position s %s.", tokens[4] );
	// };
	// devc->horiz_triggerpos = trigger_pos;

	// sr_dbg("Current horizontal trigger position %.10f.", devc->horiz_triggerpos);

	// /* Trigger slope. */
	// cmd = g_strdup_printf("%s:TRSL?", devc->trigger_source);
	// g_free(devc->trigger_slope);
	// devc->trigger_slope = NULL;
	// res = sr_scpi_get_string(sdi->conn, cmd, &devc->trigger_slope);
	// g_free(cmd);
	// if (res != SR_OK)
	// 	return SR_ERR;
	// sr_dbg("Current trigger slope: %s.", devc->trigger_slope);

	// /* Trigger level, only when analog channel. */
	// if (g_str_has_prefix(tokens[2], "C")) {
	// 	cmd = g_strdup_printf("%s:TRLV?", devc->trigger_source);
	// 	res = sr_scpi_get_float(sdi->conn, cmd, &devc->trigger_level);
	// 	g_free(cmd);
	// 	if (res != SR_OK)
	// 		return SR_ERR;
	// 	sr_dbg("Current trigger level: %g.", devc->trigger_level);
	// }

	return SR_OK;
}
