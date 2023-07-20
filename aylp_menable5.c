#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "anyloop.h"
#include "logging.h"
#include "xalloc.h"
#include "aylp_menable5.h"

// we currently assume that each pixel is 1 byte. do we want to support other
// formats, like color or 10-bit packed? we'd need to output other types ...
// like maybe a gsl_matrix_long

// max tries to get a frame
static const size_t max_tries = 10000;

int aylp_menable5_init(struct aylp_device *self)
{
	int err;
	self->device_data = xcalloc(1, sizeof(struct aylp_menable5_data));
	struct aylp_menable5_data *data = self->device_data;
	// attach methods
	self->process = &aylp_menable5_process;
	self->close = &aylp_menable5_close;

	// set default params
	data->pitch = 0.0;

	// parse the params json into our data struct
	if (!self->params) {
		log_error("No params object found.");
		return -1;
	}
	json_object_object_foreach(self->params, key, val) {
		if (key[0] == '_') {
			// keys starting with _ are comments
		} else if (!strcmp(key, "width")) {
			data->width = json_object_get_uint64(val);
			log_trace("width = %llu", data->width);
		} else if (!strcmp(key, "height")) {
			data->height = json_object_get_uint64(val);
			log_trace("height = %llu", data->height);
		} else if (!strcmp(key, "pitch")) {
			data->pitch = json_object_get_double(val);
			log_trace("pitch = %E", data->pitch);
		} else {
			log_warn("Unknown parameter \"%s\"", key);
		}
	}
	// make sure we didn't miss any params
	if (!data->width || !data->height) {
		log_error("You must provide nonzero width and height params");
		return -1;
	}

	// filling in some parameters:
	// we only need one subbuf with whatever max size
	data->creation.maxsize = UINT64_MAX;
	data->creation.subbufs = 1;
	// and we need to make enough room for that subbuf
	data->memory.length = data->width * data->height;
	data->memory.subnr = 0;
	// .start and .headnr uninitialized
	// we'll want to capture in blocking mode, indefinitely
	data->control.mode = DMA_BLOCKINGMODE;
	data->control.timeout = 1000;
	data->control.transfer_todo = GRAB_INFINITE;
	data->control.chan = 0;
	data->control.start_buf = 0;
	data->control.dma_dir = MEN_DMA_DIR_DEVICE_TO_CPU;
	// .head uninitialized

	// open the /dev file
	char fgpath[] = "/dev/menable0";	// TODO: json?
	data->fg = open(fgpath, O_RDWR);
	if (data->fg == -1) {
		log_error("Couldn't open %s: %s\n", fgpath, strerror(errno));
		return -1;
	}

	// allocate memory on the kernel side
	data->memory.headnr = (unsigned int)ioctl(
		data->fg,
		MEN_IOC(ALLOCATE_VIRT_BUFFER, data->creation),
		&data->creation
	);
	if ((int)data->memory.headnr < 0) {
		log_error("allocate_virt_buffer failed: %s\n",
			strerror(errno)
		);
		return -1;
	}
	log_debug("Got head number %d\n", data->memory.headnr);

	// allocate our own buffer
	data->fb = xcalloc_type(gsl_matrix_uchar, data->width, data->height);
	data->memory.start = (unsigned long)data->fb->data;
	// data->memory is now fully initialized

	// tell the driver about our buffer
	err = ioctl(
		data->fg,
		MEN_IOC(ADD_VIRT_USER_BUFFER, data->memory),
		&data->memory
	);
	if (err) {
		log_error("add_virt_user_buffer failed: %s\n", strerror(errno));
		return -1;
	}

	// start the capture by sending the control parameters
	data->control.head = data->memory.headnr;
	err = ioctl(
		data->fg,
		MEN_IOC(FG_START_TRANSFER, data->control),
		&data->control
	);
	if (err) {
		log_error("fg_start_transfer failed: %s\n", strerror(errno));
		return -1;
	}

	// set types and units
	self->type_in = AYLP_T_ANY;
	self->units_in = AYLP_U_ANY;
	self->type_out = AYLP_T_MATRIX_UCHAR;
	self->units_out = AYLP_U_COUNTS;

	return 0;
}


int aylp_menable5_process(struct aylp_device *self, struct aylp_state *state)
{
	int err;
	struct aylp_menable5_data *data = self->device_data;
	struct men_io_bufidx bufidx = {
		.headnr = data->memory.headnr,
		.index = data->memory.subnr,
	};
	err = ioctl(data->fg, MEN_IOC(UNLOCK_BUFFER_NR,bufidx), &bufidx);
	if (UNLIKELY(err)) {
		log_error("Unlock failed: %s\n", strerror(errno));
		return err;
	}
	size_t i;
	for (i = 0; i < max_tries; i++) {
		struct handshake_frame hand = {
			.head = data->memory.headnr,
			.mode = SEL_ACT_IMAGE
		};
		err = ioctl(data->fg,
			MEN_IOC(GET_HANDSHAKE_DMA_BUFFER,hand), &hand
		);
		if (UNLIKELY(err)) {
			log_error("Handshake failed: %s\n", strerror(errno));
			return err;
		}
		if (hand.frame == -12) {
			// frame not ready
			sched_yield();
		} else {
			break;
		}
	}
	if (UNLIKELY(i == max_tries)) {
		log_error("Hit max tries (%llu) for frame read", max_tries);
		return -1;
	}
	err = ioctl(data->fg, MEN_IOC(DMA_LENGTH,bufidx), &bufidx);
	if (UNLIKELY(err)) {
		log_error("Get length failed: %s\n", strerror(errno));
		return err;
	}
	// yes, I know, unions are a thing, but I'm using the driver's struct
	// verbatim, and this is fun and cursed ;)
	// (men_io_bufidx is technically a union of the struct and a size_t)
	// and apparently you multiply by 4 for some reason (I think the
	// DMA_LENGTH is a count of number of longs)
	size_t imglen = *(size_t *)(void *)&bufidx * 4;
	log_trace("Got image of length %lu", imglen);
	if (data->height * data->width != imglen) {
		log_error("Image is %llu long, but you specified width of %llu "
			"and height of %llu; run sdk_init again?",
			imglen, data->width, data->height
		);
		return -1;
	}
	// zero-copy update of pipeline state
	state->matrix_uchar = data->fb;
	// housekeeping on the header
	state->header.type = self->type_out;
	state->header.units = self->units_out;
	state->header.log_dim.y = data->height;
	state->header.log_dim.x = data->width;
	state->header.pitch.y = data->pitch;
	state->header.pitch.x = data->pitch;
	return 0;
}


int aylp_menable5_close(struct aylp_device *self)
{
	struct aylp_menable5_data *data = self->device_data;
	ioctl(data->fg, MEN_IOC(FG_STOP_CMD,0), data->control.chan);
	ioctl(data->fg, MEN_IOC(FREE_VIRT_BUFFER,0), data->memory.headnr);
	xfree_type(gsl_matrix_uchar, data->fb);
	xfree(data);
	return 0;
}

