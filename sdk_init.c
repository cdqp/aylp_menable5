#include <stdio.h>
#include <stdlib.h>

#include "basler_fg.h"

int main(int argc, char **argv)
{
	Fg_Struct *fg = NULL;
	int camPort = PORT_A;

	if (argc != 3) {
		puts("Usage: sdk_init width_in_px height_in_px\n");
		return -1;
	}

	unsigned long width = strtoul(argv[1], 0, 0);
	unsigned long height = strtoul(argv[2], 0, 0);
	printf("Okay, initializing for %lu by %lu capture\n", width, height);

	int status = Fg_InitLibraries(0);
	if (status != FG_OK) {
		fprintf(stderr, "Failed to initialize libraries\n");
		return FG_ERROR;
	}

	fg = Fg_Init("Acq_SingleFullAreaGray", 0);
	if (fg == NULL) {
		fprintf(stderr, "error in Fg_Init: %s\n",
			Fg_getLastErrorDescription(NULL)
		);
		return FG_ERROR;
	}

	dma_mem *mem = Fg_AllocMemEx(fg, width*height, 1);
	if (!mem) {
		fprintf(stderr, "error in Fg_AllocMemEx: %s\n",
			Fg_getLastErrorDescription(fg)
		);
		Fg_FreeGrabber(fg);
		return FG_ERROR;
	}

	// Image width of the acquisition window.
	if (Fg_setParameter(fg, FG_WIDTH, &width, camPort) < 0) {
		fprintf(stderr, "Fg_setParameter(FG_WIDTH) failed: %s\n",
			Fg_getLastErrorDescription(fg)
		);
		Fg_FreeMemEx(fg, mem);
		Fg_FreeGrabber(fg);
		return FG_ERROR;
	}
	// Image height of the acquisition window.
	if (Fg_setParameter(fg, FG_HEIGHT, &height, camPort) < 0) {
		fprintf(stderr, "Fg_setParameter(FG_HEIGHT) failed: %s\n",
			Fg_getLastErrorDescription(fg)
		);
		Fg_FreeMemEx(fg, mem);
		Fg_FreeGrabber(fg);
		return FG_ERROR;
	}

	int bitAlignment = FG_LEFT_ALIGNED;
	if (Fg_setParameter(fg, FG_BITALIGNMENT, &bitAlignment, camPort) < 0) {
		fprintf(stderr, "Fg_setParameter(FG_BITALIGNMENT) failed: %s\n",
			Fg_getLastErrorDescription(fg)
		);
		Fg_FreeMemEx(fg, mem);
		Fg_FreeGrabber(fg);
		return FG_ERROR;
	}

	//puts("FG_ACQUIREEX CALL"); fflush(stdout);
	// start an acquisition for one frame
	if (Fg_AcquireEx(fg, camPort, 1, ACQ_BLOCK, mem) < 0) {
		fprintf(stderr, "Fg_AcquireEx() failed: %s\n",
			Fg_getLastErrorDescription(fg)
		);
		Fg_FreeMemEx(fg, mem);
		Fg_FreeGrabber(fg);
		return FG_ERROR;
	}
	//puts("FG_ACQUIREEX RET"); fflush(stdout);

	// stopAcquire somehow ruins the no-sdk code
	//Fg_stopAcquire(fg, camPort);
	Fg_FreeMemEx(fg, mem);
	// FreeGrabber also somehow ruins the no-sdk code
	//Fg_FreeGrabber(fg);
	Fg_FreeLibraries();

	puts("Done.\n");
	return FG_OK;
}

