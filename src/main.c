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
	printheadline();
	
	IosPatch_RUNTIME(PATCH_WII, PATCH_ISFS_PERMISSIONS | PATCH_ES_IDENTIFY | PATCH_HASH_CHECK | PATCH_NEW_HASH_CHECK, true);
	Install_patched_MIOS(0x101, 10, true, 256);
	VIDEO_WaitVSync();
	
	Close_SD();
	Close_USB();
	Reboot();

	return 0;
}
