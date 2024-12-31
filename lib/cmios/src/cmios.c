#include <string.h>
#include <unistd.h>
#include <gccore.h>
#include "console.h"

extern int execFromMem(void* data);

static void callMIOS(void) {
	// restore patched entry point
	u32* entry = (u32*) 0x800037fc;
	*entry = 0x81300200;
	DCFlushRange(entry, 32);
	ICInvalidateRange(entry, 32);
	// simulate boot
	__asm__(
		"bl DCDisable\n"
		"bl ICDisable\n"
		"li %r3, 0\n"
		"mtsrr1 %r3\n"
		"li %r4, 0x3400\n"
		"mtsrr0 %r4\n"
		"rfi\n"
	);
}

int main(int argc, char **argv) {
	char* legacyMagic = (char*)0x807fffe0;
    consoleInit(); // some sort of video init is *required* by hardware for input detection

	// old cmios behaviour
	if (strncmp(legacyMagic, "gchomebrew dol", 32) == 0) {
		// consume magic value
		*legacyMagic = 0;
		DCFlushRange(legacyMagic, 32);
		ICInvalidateRange(legacyMagic, 32);
		// branch to payload (in stale memory)
		execFromMem((void*)0x80800000);
		sleep(3);
	}

	// normal startup (if no homebrew)
	callMIOS();
	return 0;
}
