#include <stdio.h>
#include <time.h>

#include <basler_fg.h>

int main()
{
	Fg_Struct *fg = NULL;
	int camPort = PORT_A;
	frameindex_t nrOfFrames = GRAB_INFINITE;
	frameindex_t nbBuffers = 1;
	// not sure if the acquisition that we start here has to have anything
	// to do with the acquisition we do later with our own code
	unsigned int width = 80;
	unsigned int height = 80;
	int samplePerPixel = 1;
	size_t bytePerSample = 1;

	int status = Fg_InitLibraries(0);
	if (status != FG_OK) {
		fprintf(stderr, "Failed to initialize libraries\n");
		return FG_ERROR;
	}

	int boardNr = 0;
	char *applet = "Acq_SingleFullAreaGray";

	fg = Fg_Init(applet, boardNr);
	if (fg == NULL) {
		fprintf(stderr, "error in Fg_Init: %s\n",
			Fg_getLastErrorDescription(NULL)
		);
		return FG_ERROR;
	}

	// Calculate buffer size (careful to avoid integer arithmetic
	// overflows!) and allocate memory.
	size_t totalBufferSize = (size_t) width * height * samplePerPixel
		* bytePerSample * nbBuffers;
	dma_mem *mem = Fg_AllocMemEx(fg, totalBufferSize, nbBuffers);
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

	puts("FG_ACQUIREEX CALL"); fflush(stdout);
	if (Fg_AcquireEx(fg,camPort,nrOfFrames,ACQ_BLOCK,mem) < 0) {
		fprintf(stderr, "Fg_AcquireEx() failed: %s\n",
			Fg_getLastErrorDescription(fg)
		);
		Fg_FreeMemEx(fg, mem);
		Fg_FreeGrabber(fg);
		return FG_ERROR;
	}
	puts("FG_ACQUIREEX RET"); fflush(stdout);

	// stopAcquire somehow ruins the no-sdk code
	//Fg_stopAcquire(fg, camPort);
	Fg_FreeMemEx(fg, mem);
	// FreeGrabber also somehow ruins the no-sdk code
	//Fg_FreeGrabber(fg);
	Fg_FreeLibraries();

	return FG_OK;
}

