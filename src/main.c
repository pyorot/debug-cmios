#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>

#include "rtip.h"
#include "IOSPatcher.h"
#include "wad.h"
#include "tools.h"

int main(int argc, char* argv[])
{
	Init_Console();
	printf("\x1b[%u;%um", 37, false);

	PAD_Init();
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);					

	printheadline();
	
	s32 ret = IosPatch_RUNTIME(PATCH_WII, PATCH_ISFS_PERMISSIONS | PATCH_HASH_CHECK | PATCH_NEW_HASH_CHECK, false);
	if (ret >= 0) {
		printf("\nPress Y/1 to install the cMIOS or X/2 for vanilla MIOS; other to exit.\n");
		u32 pressed, pressedGC;
		waitforbuttonpress(&pressed, &pressedGC);
		if (pressed == WPAD_BUTTON_1 || pressedGC == PAD_BUTTON_Y) {
			Install_patched_MIOS(0x101, 10, true, 10);
			printf("Press any button to exit...\n"); waitforbuttonpress(NULL, NULL);
		} else if (pressed == WPAD_BUTTON_2 || pressedGC == PAD_BUTTON_X) {
			Install_patched_MIOS(0x101, 10, false, 10);
			printf("Press any button to exit...\n"); waitforbuttonpress(NULL, NULL);
		}
	} else {
		printf("\nNo AHB access; use the original meta.xml and Homebrew Channel v1.1+.\n");
		printf("Press any button to exit...\n"); waitforbuttonpress(NULL, NULL);
	}

	Close_SD();
	Close_USB();
	printf("Exiting...\n"); VIDEO_WaitVSync();
	Reboot();

	return 0;
}
