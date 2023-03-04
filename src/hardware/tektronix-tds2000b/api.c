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
#include <string.h>
#include "scpi.h"
#include <math.h>
#include "protocol.h"

static struct sr_dev_driver tektronix_tds2000b_driver_info;


static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_OSCILLOSCOPE,
	// SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	SR_CONF_TIMEBASE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_SLOPE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_LEVEL | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_HORIZ_TRIGGERPOS | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_NUM_HDIV | SR_CONF_GET, // | SR_CONF_LIST,
	SR_CONF_SAMPLERATE | SR_CONF_GET,
	SR_CONF_LIMIT_FRAMES | SR_CONF_GET | SR_CONF_SET,
	// SR_CONF_DATA_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	// SR_CONF_AVERAGING | SR_CONF_GET | SR_CONF_SET,
	// SR_CONF_AVG_SAMPLES | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const uint32_t devopts_cg_analog[] = {
	SR_CONF_NUM_VDIV | SR_CONF_GET,
	SR_CONF_VDIV | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_COUPLING | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_PROBE_FACTOR | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	// SR_CONF_DATA_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

// TODO: fixup for comp
#define vdiv_nums 11
static const uint64_t vdivs[][2] = {
	/* millivolts */
	{ 2, 1000 },
	{ 5, 1000 },
	{ 10, 1000 },
	{ 20, 1000 },
	{ 50, 1000 },
	{ 100, 1000 },
	{ 200, 1000 },
	{ 500, 1000 },
	/* volts */
	{ 1, 1 },
	{ 2, 1 },
	{ 5, 1 },
};

// TODO: fixup per model
#define timebase_nums 32
static const uint64_t timebases[][2] = {
	/* nanoseconds */
	{ 25, 10000000000 },
	{ 5, 1000000000 },
	{ 10, 1000000000 },
	{ 25, 1000000000 },
	{ 50, 1000000000 },
	{ 100, 1000000000 },
	{ 250, 1000000000 },
	{ 500, 1000000000 },
	/* microseconds */
	{ 1, 1000000 },
	{ 25, 10000000 },
	{ 5, 1000000 },
	{ 10, 1000000 },
	{ 25, 1000000 },
	{ 50, 1000000 },
	{ 100, 1000000 },
	{ 250, 1000000 },
	{ 500, 1000000 },
	/* milliseconds */
	{ 1, 1000 },
	{ 25, 10000 },
	{ 5, 1000 },
	{ 10, 1000 },
	{ 25, 1000 },
	{ 50, 1000 },
	{ 100, 1000 },
	{ 250, 1000 },
	{ 500, 1000 },
	/* seconds */
	{ 1, 1 },
	{ 25, 10 },
	{ 5, 1 },
	{ 10, 1 },
	{ 25, 1 },
	{ 50, 1 },
};

static const char *coupling[] = {
	"AC",
	"DC",
	"GND",
};

static const uint64_t probe_factor[] = {
	// TTODO: 0.2x
	1, 2, 5, 10, 50, 100, 1000
};

static const char *trigger_slopes[] = {
	"r", "f",
};


// TODO: 2 vs 4 ch
static const char *trigger_sources[] = {
	"CH1", "CH2", 
	"CH3", "CH4",
	"Ext", "Ext /5", "Ext /10", "AC Line",
};


static const struct mini_device_spec device_models[] = {
	{ "TDS 2002B", 2 },
	{ "TDS 2004B", 4 },
	{ "TDS 2012B", 2 },
	{ "TDS 2014B", 4 },
	{ "TDS 2022B", 2 },
	{ "TDS 2024B", 4 },
};

static const char *TEKTRONIX = "TEKTRONIX";
static const char *Tektronix = "Tektronix";

static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_scpi_hw_info *hw_info;
	const struct mini_device_spec *device;
	const struct scpi_command *cmdset;
	struct sr_channel *ch;
	struct sr_channel_group *cg;
	const char *command;
	unsigned int i, ch_idx;
	char tmp[16];
	gchar *channel_name;

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	
	if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK) {
		sr_info("Couldn't get IDN response, retrying.");
		sr_scpi_close(scpi);
		sr_scpi_open(scpi);
		if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK) {
			sr_info("Couldn't get IDN response.");
			goto error;
		}
	}

	device = NULL;
	for (i = 0; i < ARRAY_SIZE(device_models); i++) {
		if (g_ascii_strcasecmp(hw_info->manufacturer,
				TEKTRONIX) != 0)
			continue;
		if (g_ascii_strcasecmp(hw_info->model,
				device_models[i].model) != 0)
			continue;
		device = &device_models[i];
		break;
	}

	if (!device)
		goto error;

	sdi = g_malloc0(sizeof(*sdi));
	sdi->vendor = g_strdup(Tektronix);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->conn = scpi;
	sdi->driver = &tektronix_tds2000b_driver_info;
	sdi->inst_type = SR_INST_SCPI;

	devc = g_malloc0(sizeof(*devc));
	devc->model = device;
	sdi->priv = devc;
	devc->buffer = g_malloc(5000);
	devc->limit_frames = 1;

	sr_scpi_hw_info_free(hw_info);


	devc->analog_groups = g_malloc0(sizeof(struct sr_channel_group *) *
		device->channels);

	for (i = 0; i < device->channels; i++) {
		channel_name = g_strdup_printf("CH%d", i + 1);
		ch = sr_channel_new(sdi, i, SR_CHANNEL_ANALOG, TRUE, channel_name);

		devc->analog_groups[i] = sr_channel_group_new(sdi,
			channel_name, NULL);
		devc->analog_groups[i]->channels = g_slist_append(NULL, ch);
	}

	return sdi;

error:
	sr_scpi_hw_info_free(hw_info);
	g_free(devc);
	sr_dev_inst_free(sdi);

	return NULL;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	return sr_scpi_scan(di->context, options, probe_device);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	int ret;
	struct sr_scpi_dev_inst *scpi = sdi->conn;

	if ((ret = sr_scpi_open(scpi)) < 0) {
		sr_err("Failed to open SCPI device: %s.", sr_strerror(ret));
		return SR_ERR;
	}

	if ((ret = tek_tds2000b_get_dev_cfg(sdi)) < 0) {
		sr_err("Failed to get device config: %s.", sr_strerror(ret));
		return SR_ERR;
	}

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	return sr_scpi_close(sdi->conn);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	struct sr_channel *ch;
	const char *tmp_str;
	int analog_channel = -1;
	float smallest_diff = INFINITY;
	int idx = -1;
	unsigned i;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	/* If a channel group is specified, it must be a valid one. */
	if (cg && !g_slist_find(sdi->channel_groups, cg)) {
		sr_err("Invalid channel group specified.");
		return SR_ERR;
	}

	if (cg) {
		ch = g_slist_nth_data(cg->channels, 0);
		if (!ch)
			return SR_ERR;
		if (ch->type == SR_CHANNEL_ANALOG) {
			if (ch->name[2] < '1' || ch->name[2] > '4')
				return SR_ERR;
			analog_channel = ch->name[2] - '1';
		}
	}

	switch (key) {
	case SR_CONF_NUM_HDIV:
		*data = g_variant_new_int32(8);
		break;
	case SR_CONF_NUM_VDIV:
		*data = g_variant_new_int32(vdiv_nums);
		break;
	case SR_CONF_LIMIT_FRAMES:
		*data = g_variant_new_uint64(devc->limit_frames);
		break;
	// case SR_CONF_DATA_SOURCE:
	// 	if (devc->data_source == DATA_SOURCE_SCREEN)
	// 		*data = g_variant_new_string("Screen");
	// 	else if (devc->data_source == DATA_SOURCE_HISTORY)
	// 		*data = g_variant_new_string("History");
	// 	break;
	case SR_CONF_SAMPLERATE:
		tek_tds2000b_get_dev_cfg_horizontal(sdi);
		*data = g_variant_new_uint64(devc->samplerate);
		break;
	case SR_CONF_TRIGGER_SOURCE:
		*data = g_variant_new_string(devc->trigger_source);
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if (!g_ascii_strncasecmp(devc->trigger_slope, "RISE", 4)) {
			tmp_str = "r";
		} else if (!g_ascii_strncasecmp(devc->trigger_slope, "FALL", 4)) {
			tmp_str = "f";
		} else {
			sr_dbg("Unknown trigger slope: '%s'.", devc->trigger_slope);
			return SR_ERR_NA;
		}
		*data = g_variant_new_string(tmp_str);
		break;
	case SR_CONF_TRIGGER_LEVEL:
		*data = g_variant_new_double(devc->trigger_level);
		break;
	case SR_CONF_HORIZ_TRIGGERPOS:
		*data = g_variant_new_double(devc->horiz_triggerpos);
		break;
	case SR_CONF_TIMEBASE:
		for (i = 0; i < timebase_nums; i++) {
			float tb, diff;

			tb = (float)timebases[i][0] / timebases[i][1];
			diff = fabs(devc->timebase - tb);
			if (diff < smallest_diff) {
				smallest_diff = diff;
				idx = i;
			}
		}
		if (idx < 0) {
			sr_dbg("Negative timebase index: %d.", idx);
			return SR_ERR_NA;
		}
		*data = g_variant_new("(tt)", timebases[idx][0],
			timebases[idx][1]);
		break;
	case SR_CONF_VDIV:
		if (analog_channel < 0) {
			sr_dbg("Negative analog channel: %d.", analog_channel);
			return SR_ERR_NA;
		}
		for (i = 0; i < ARRAY_SIZE(vdivs); i++) {
			float vdiv = (float)vdivs[i][0] / vdivs[i][1];
			float diff = fabsf(devc->vdiv[analog_channel] - vdiv);
			if (diff < smallest_diff) {
				smallest_diff = diff;
				idx = i;
			}
		}
		if (idx < 0) {
			sr_dbg("Negative vdiv index: %d.", idx);
			return SR_ERR_NA;
		}
		*data = g_variant_new("(tt)", vdivs[idx][0], vdivs[idx][1]);
		break;
	case SR_CONF_COUPLING:
		if (analog_channel < 0) {
			sr_dbg("Negative analog channel: %d.", analog_channel);
			return SR_ERR_NA;
		}
		*data = g_variant_new_string(devc->coupling[analog_channel]);
		break;
	case SR_CONF_PROBE_FACTOR:
		if (analog_channel < 0) {
			sr_dbg("Negative analog channel: %d.", analog_channel);
			return SR_ERR_NA;
		}
		*data = g_variant_new_uint64(devc->attenuation[analog_channel]);
		break;
	// case SR_CONF_AVERAGING:
	// 	*data = g_variant_new_boolean(devc->average_enabled);
	// 	break;
	// case SR_CONF_AVG_SAMPLES:
	// 	*data = g_variant_new_uint64(devc->average_samples);
		// break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	uint64_t p;
	double t_dbl;
	int i;
	int ret, idx;
	const char *tmp_str;
	char buffer[16];
	char *cmd = "";
	char cmd4[4];

	devc = sdi->priv;

	/* If a channel group is specified, it must be a valid one. */
	if (cg && !g_slist_find(sdi->channel_groups, cg)) {
		sr_err("Invalid channel group specified.");
		return SR_ERR;
	}

	ret = SR_OK;
	switch (key) {
	case SR_CONF_LIMIT_FRAMES:
		devc->limit_frames = g_variant_get_uint64(data);
		sr_info("Getting frames limit of %li",  g_variant_get_uint64(data));
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if ((idx = std_str_idx(data, ARRAY_AND_SIZE(trigger_slopes))) < 0)
			return SR_ERR_ARG;
		g_free(devc->trigger_slope);
		devc->trigger_slope = g_strdup((trigger_slopes[idx][0] == 'r') ? "RISE" : "FALL");
		return tek_tds2000b_config_set(sdi, "TRIG:MAI:EDGE:SLO %s", devc->trigger_slope);
	case SR_CONF_HORIZ_TRIGGERPOS:
		t_dbl = g_variant_get_double(data);
		if (t_dbl < 0.0 || t_dbl > 1.0) {
			sr_err("Invalid horiz. trigger position: %g.", t_dbl);
			return SR_ERR;
		}
		devc->horiz_triggerpos = t_dbl;
		/* We have the trigger offset as a percentage of the frame, but
		 * need to express this in seconds. */
		t_dbl = -(devc->horiz_triggerpos - 0.5) * devc->timebase * (10);//devc->num_timebases;
		// g_ascii_formatd(buffer, sizeof(buffer), "%.6f", t_dbl);
		return tek_tds2000b_config_set(sdi, "hor:mai:pos %.3e", t_dbl);
	case SR_CONF_TRIGGER_LEVEL:
		if (!strcmp(devc->trigger_source, "AC Line"))
			sr_err("Can't set level on AC line trigger, ignoring");
			return SR_ERR;

		t_dbl = g_variant_get_double(data);
		g_ascii_formatd(buffer, sizeof(buffer), "%.3f", t_dbl);
		ret = tek_tds2000b_config_set(sdi, "TRIG:MAI:LEV %s", buffer);
		if (ret == SR_OK)
			devc->trigger_level = t_dbl;
		break;
	case SR_CONF_TIMEBASE:
		if ((idx = std_u64_tuple_idx(data, timebases, timebase_nums)) < 0)
			return SR_ERR_ARG;
		devc->timebase = (float)timebases[idx][0] / timebases[idx][1];
		ret = tek_tds2000b_config_set(sdi, "hor:sca %.1e", devc->timebase);
		if (ret == SR_OK)
			tek_tds2000b_get_dev_cfg_horizontal(sdi);
		return ret;
	case SR_CONF_TRIGGER_SOURCE:
		if ((idx = std_str_idx(data, ARRAY_AND_SIZE(trigger_sources))) < 0)
			return SR_ERR_ARG;
		g_free(devc->trigger_source);
		devc->trigger_source = g_strdup(trigger_sources[idx]);
		if (!strcmp(devc->trigger_source, "AC Line"))
		{
			// ONLY set edge trigger, as only edge trigger supports this
			// TODO: raise error when not edge source	
			return tek_tds2000b_config_set(sdi, "TRIG:mai:edge:sou line");
		}
		else if (!strcmp(devc->trigger_source, "Ext /5"))
			tmp_str = "EXT5";
		else if (!strcmp(devc->trigger_source, "Ext /10"))
			tmp_str = "EXT10";
		else
			tmp_str = (char *)devc->trigger_source;
			// TODO: pulse and video
		return tek_tds2000b_config_set(sdi, "TRIG:mai:edge:sou %s", tmp_str);
	case SR_CONF_VDIV:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((i = std_cg_idx(cg, devc->analog_groups, devc->model->channels)) < 0)
			return SR_ERR_ARG;
		if ((idx = std_u64_tuple_idx(data, ARRAY_AND_SIZE(vdivs))) < 0)
			return SR_ERR_ARG;
		devc->vdiv[i] = (float)vdivs[idx][0] / vdivs[idx][1];
		ret = tek_tds2000b_config_set(sdi, "CH%d:SCA %.2e", i + 1, (double)devc->vdiv[i]);
		return ret;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((i = std_cg_idx(cg, devc->analog_groups, devc->model->channels)) < 0)
			return SR_ERR_ARG;
		if ((idx = std_str_idx(data, ARRAY_AND_SIZE(coupling))) < 0)
			return SR_ERR_ARG;
		g_free(devc->coupling[i]);
		devc->coupling[i] = g_strdup(coupling[idx]);
		strncpy(cmd4, devc->coupling[i], 3);
		cmd4[3] = 0;
		return tek_tds2000b_config_set(sdi, "CH%d:COUP %s", i + 1, cmd4);
	case SR_CONF_PROBE_FACTOR:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		if ((i = std_cg_idx(cg, devc->analog_groups, devc->model->channels)) < 0)
			return SR_ERR_ARG;
		if ((idx = std_u64_idx(data, ARRAY_AND_SIZE(probe_factor))) < 0)
			return SR_ERR_ARG;
		p = g_variant_get_uint64(data);
		devc->attenuation[i] = probe_factor[idx];
		ret = tek_tds2000b_config_set(sdi, "CH%d:PROBE %" PRIu64, i + 1, p);
		if (ret == SR_OK)
			tek_tds2000b_get_dev_cfg_vertical(sdi);
		return ret;
	// case SR_CONF_DATA_SOURCE:
	// 	tmp_str = g_variant_get_string(data, NULL);
	// 	if (!strcmp(tmp_str, "Display"))
	// 		devc->data_source = DATA_SOURCE_SCREEN;
	// 	else if (devc->model->series->protocol >= SPO_MODEL
	// 		&& !strcmp(tmp_str, "History"))
	// 		devc->data_source = DATA_SOURCE_HISTORY;
	// 	else {
	// 		sr_err("Unknown data source: '%s'.", tmp_str);
	// 		return SR_ERR;
	// 	}
	// 	break;
	// case SR_CONF_SAMPLERATE:
		// ret = tek_tds2000b_config_set(sdi, "HOR:SCA %" PRIu64, g_variant_get_uint64(data));
		// if (ret == SR_OK)
			// tek_tds2000b_get_dev_cfg_horizontal(sdi);
		// return ret;
	// case SR_CONF_AVERAGING:
	// 	devc->average_enabled = g_variant_get_boolean(data);
	// 	sr_dbg("%s averaging", devc->average_enabled ? "Enabling" : "Disabling");
	// 	break;
	// case SR_CONF_AVG_SAMPLES:
	// 	devc->average_samples = g_variant_get_uint64(data);
	// 	sr_dbg("Setting averaging rate to %" PRIu64, devc->average_samples);
	// 	break;	
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	devc = (sdi) ? sdi->priv : NULL;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		if (!cg)
			return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
		if (!devc)
			return SR_ERR_ARG;
		if (std_cg_idx(cg, devc->analog_groups, devc->analog_channels) < 0)
			return SR_ERR_ARG;
		*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_analog));
		return SR_OK;
		break;
	case SR_CONF_COUPLING:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		*data = g_variant_new_strv(ARRAY_AND_SIZE(coupling));
		break;
	case SR_CONF_PROBE_FACTOR:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		*data = std_gvar_array_u64(ARRAY_AND_SIZE(probe_factor));
		break;
	case SR_CONF_VDIV:
		if (!devc)
			/* Can't know this until we have the exact model. */
			return SR_ERR_ARG;
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		*data = std_gvar_tuple_array(vdivs, vdiv_nums);
		break;
	case SR_CONF_TIMEBASE:
		if (!devc)
			/* Can't know this until we have the exact model. */
			return SR_ERR_ARG;
		*data = std_gvar_tuple_array(timebases, timebase_nums);
		break;
	case SR_CONF_TRIGGER_SOURCE:
		// if (!devc)
		// 	/* Can't know this until we have the exact model. */
		// 	return SR_ERR_ARG;
		// *data = g_variant_new_strv(trigger_sources,
		// 	devc->model->has_digital ? ARRAY_SIZE(trigger_sources) : 5);
		*data = g_variant_new_strv(ARRAY_AND_SIZE(trigger_sources));
		break;
	case SR_CONF_TRIGGER_SLOPE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(trigger_slopes));
		break;
	// case SR_CONF_DATA_SOURCE:
	// 	if (!devc)
	// 		/* Can't know this until we have the exact model. */
	// 		return SR_ERR_ARG;
	// 	switch (devc->model->series->protocol) {
	// 	/* TODO: Check what must be done here for the data source buffer sizes. */
	// 	case NON_SPO_MODEL:
	// 		*data = g_variant_new_strv(data_sources, ARRAY_SIZE(data_sources) - 1);
	// 		break;
	// 	case SPO_MODEL:
	// 	case ESERIES:
	// 		*data = g_variant_new_strv(ARRAY_AND_SIZE(data_sources));
	// 		break;
	// 	}
	// 	break;
	case SR_CONF_NUM_HDIV:
		*data = g_variant_new_int32(10);//devc->model->series->num_horizontal_divs);
		break;
	case SR_CONF_NUM_VDIV:
		*data = g_variant_new_int32(8);//devc->model->series->num_horizontal_divs);
		break;
	// case SR_CONF_AVG_SAMPLES:
	// 	*data = std_gvar_array_u64(ARRAY_AND_SIZE(averages));
	// 	break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct sr_scpi_dev_inst *scpi;
	struct dev_context *devc;
	struct sr_channel *ch;
	GSList *l, *d;

	scpi = sdi->conn;
	devc = sdi->priv;

	devc->num_frames = 0;

	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (ch->enabled)
			devc->enabled_channels = g_slist_append(
				devc->enabled_channels, ch);
		if (ch->enabled != devc->analog_channels[ch->index]) {
			/* Enabled channel is currently disabled, or vice versa. */
			if (tek_tds2000b_config_set(sdi, "SEL:CH%d %s", ch->index + 1,
				ch->enabled ? "ON" : "OFF") != SR_OK)
				return SR_ERR;
			devc->analog_channels[ch->index] = ch->enabled;
		}
	}
	if (!devc->enabled_channels)
		return SR_ERR;

	
	tek_tds2000b_get_dev_cfg_horizontal(sdi); // TODO:!

	if (tek_tds2000b_config_set(sdi, "ACQ:STATE RUN") != SR_OK)
		return SR_ERR;

	sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 100,
		tek_tds2000b_receive, (void *) sdi);

	std_session_send_df_header(sdi);

	devc->channel_entry = devc->enabled_channels;

	sr_info("Doing initial start");

	if (tek_tds2000b_capture_start(sdi) != SR_OK)
		return SR_ERR;

	sr_info("Marking fame begin");
	/* Start of first frame. */
	std_session_send_df_frame_begin(sdi);

	sr_info("Marking done");

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;

	devc = sdi->priv;

	std_session_send_df_end(sdi);

	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;
	scpi = sdi->conn;
	sr_scpi_source_remove(sdi->session, scpi);

	return SR_OK;
}

static struct sr_dev_driver tektronix_tds2000b_driver_info = {
	.name = "tektronix-tds2000b",
	.longname = "Tektronix TDS2000B",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(tektronix_tds2000b_driver_info);
