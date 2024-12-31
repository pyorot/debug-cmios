#include <string.h>
#include <unistd.h>
#include <gccore.h>
// inject swiss.dol binary (placed in lib/cmios/data folder) into cmios
#include "swiss_dol.h"
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

// configures DVD drive (needed for disc change and exit)
static void resetDI(void) {
	vu32 *pi_cmd = (u32*) 0xCC003024;
	vu32 *ios_di_sync = (u32*) 0x800030F8;
	vu32 *ios_sync_base = (u32*) 0x800030E0;
	vu32 *phys_ios_di_sync = (u32*) 0xC00030F8;

	*pi_cmd |= 0b101; // GameCube hardware registers: not DI reset, MEM reset, SYS reset rsp.
	// SYS reset avoids crash and not DI reset avoids "detecting devices" double drive spin-up in Swiss
	*ios_di_sync = 1; // DI reset compatibility bit
	DCFlushRange((void *) ios_sync_base, 32);
	while (*phys_ios_di_sync != 0); // wait for Starlet
}

int main(int argc, char **argv) {
	char* legacyMagic = (char*)0x807fffe0;
    consoleInit(); // some sort of video init is *required* by hardware for input detection

	// launch bundled homebrew if Y-hold detected
	if (padScanOnNextFrame() && (PAD_ButtonsHeld(0) & PAD_BUTTON_Y)) {
		// branch to payload (in cmios binary)
		resetDI(); // needed with current Swiss to prevent double drive restart
		execFromMem((void*)swiss_dol);
		sleep(3);
	} else

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
