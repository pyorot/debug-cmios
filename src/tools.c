#include <gccore.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>

#include "tools.h"

#include <ogc/usbstorage.h>

extern DISC_INTERFACE __io_usbstorage;

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void Reboot()
{
	if (*(u32*)0x80001800) exit(0);
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

void waitforbuttonpress(u32 *out, u32 *outGC)
{
	u32 pressed = 0;
	u32 pressedGC = 0;

	while (true)
	{
		WPAD_ScanPads();
		pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);

		PAD_ScanPads();
		pressedGC = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);

		if (pressed || pressedGC) 
		{
			if (pressedGC)
			{
				// Without waiting you can't select anything
				usleep (20000);
			}
			if (out) *out = pressed;
			if (outGC) *outGC = pressedGC;
			return;
		}
	}
}

void Init_Console()
{
	// Initialise the video system
	VIDEO_Init();
	
	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);
	
	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);
	
	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	// Set console parameters
    int x = 24, y = 32, w, h;
    w = rmode->fbWidth - (32);
    h = rmode->xfbHeight - (48);

    // Initialize the console - CON_InitEx works after VIDEO_ calls
	CON_InitEx(rmode, x, y, w, h);

	// Clear the garbage around the edges of the console
    VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
}

s32 Init_SD()
{
	if (!__io_wiisd.startup())
	{
		printf("SD card error, press any button to exit...\n");
		return -1;
	}
	
	if (!fatMountSimple("sd", &__io_wiisd))
	{
		printf("FAT error, press any button to exit...\n");
		return -1;
	}

	return 0;
}

void Close_SD()
{
	fatUnmount("sd");
	//__io_wiisd.shutdown();
}

s32 Init_USB()
{
	if (!__io_usbstorage.startup())
	{
		printf("USB storage error, press any button to exit...\n");
		return -1;		
	}
	
	if (!fatMountSimple("usb", &__io_usbstorage))
	{
		printf("FAT error, press any button to exit...\n");
		return -1;
	}
	return 0;
}

void Close_USB()
{
	fatUnmount("usb");
	//__io_usbstorage.shutdown();
}

void printheadline()
{
	int rows, cols;
	CON_GetMetrics(&cols, &rows);

	printf("Installer for Swiss (r1788) cMIOS");
	
	char buf[64];
	sprintf(buf, "IOS%u (Rev %u)\n", IOS_GetVersion(), IOS_GetRevision());
	printf("\x1B[%d;%dH", 0, cols-strlen(buf)-1);	
	printf(buf);
}

void set_highlight(bool highlight)
{
	if (highlight)
	{
		printf("\x1b[%u;%um", 47, false);
		printf("\x1b[%u;%um", 30, false);
	} else
	{
		printf("\x1b[%u;%um", 37, false);
		printf("\x1b[%u;%um", 40, false);
	}
}

void Con_ClearLine()
{
	s32 cols, rows;
	u32 cnt;

	printf("\r");
	fflush(stdout);

	/* Get console metrics */
	CON_GetMetrics(&cols, &rows);

	/* Erase line */
	for (cnt = 1; cnt < cols; cnt++) {
		printf(" ");
		fflush(stdout);
	}

	printf("\r");
	fflush(stdout);
}
