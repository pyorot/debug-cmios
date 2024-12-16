#include <string.h>
#include "cmios.h"
#include "console.h"
#include "sha1.h"

#define INJECT	// inject swiss.dol binary (placed in data folder) into cmios

#ifdef INJECT
#include "swiss_dol.h"
#endif

CMIOSTag* tag;
char* legacyMagic;

static void callMIOS(void) {
	// restore patched entry point
	unsigned long *entry = (unsigned long *) 0x800037fc;
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

// enables DVD drive reset (needed for disc change and exit)
static void resetDI(void) {
	volatile unsigned long *pi_cmd = (unsigned long *) 0xCC003024;
	volatile unsigned long *ios_di_sync = (unsigned long *) 0x800030F8;
	volatile unsigned long *ios_sync_base = (unsigned long *) 0x800030E0;
	volatile unsigned long *phys_ios_di_sync = (unsigned long *) 0xC00030F8;

	*pi_cmd |= 0b1; // trigger GameCube DI reset, MEM reset, SYS reset
	*ios_di_sync = 1; // DI reset compatibility bit
	DCFlushRange((void *) ios_sync_base, 32);
	while (*phys_ios_di_sync != 0); // wait for Starlet.
}

bool verify() {
	static u8 hash[20];
    printf(">> hashes:");
	for (int i=4; i>=0; i--) {
		SHA1((u8*)tag->payloads[i], tag->size, hash);
		printf(" %02x%02x", hash[0], hash[1]);
	}
    return memcmp(&hash, &tag->hash, 20) == 0;
}

int main(int argc, char **argv) {
	tag = (CMIOSTag*)0x80001800;
	legacyMagic = (char*)0x807fffe0;

#ifdef INJECT
	// new cmios load from injected
	if (tag->magic == 0x50155d01) {
		// clear magic value so GC disc will work after reset
		tag->magic = 0;
		DCFlushRange(tag, sizeof(CMIOSTag));
		ICInvalidateRange(tag, sizeof(CMIOSTag));
		// text
		consoleInit();
		printf("== cMIOS (inject) ==\n\npayload location: %p\n", swiss_dol);
		sleep(1);
		// branch to payload
		resetDI();
		execFromMem((u8*)swiss_dol);
		sleep(3);
	} else
#endif

	// new cmios load from file
	if ((tag->magic & 0x0fffffff) == 0xd0110ad) {
		bool ecc = tag->magic >> 28 == 1; // first character
		// clear magic value so GC disc will work after reset
		tag->magic = 0;
		DCFlushRange(tag, sizeof(CMIOSTag));
		ICInvalidateRange(tag, sizeof(CMIOSTag));
		// text
		consoleInit();
		printf("== cMIOS %s==\n", ecc ? "(file ecc) " : "(file)");
		sleep(1);
		// branch to payload if verified
		if (ecc) { correctErrors(); }
		if (verify()) {
			printf(CON_GREEN("\nOK\n"));
			sleep(3);
			resetDI();
			execFromMem(tag->payloads[0]);
		} else {
			printf(CON_RED("\nFAIL\n"));
		}
		sleep(3);
	} else

	// old cmios behaviour
	if (strncmp(legacyMagic, "gchomebrew dol", 32) == 0) {
		// clear magic value so GC disc will work after reset
		*legacyMagic = 0;
		DCFlushRange(legacyMagic, 32);
		ICInvalidateRange(legacyMagic, 32);
		// text
		consoleInit();
		printf("== cMIOS (legacy) ==\n");
		VIDEO_WaitVSync();
		// branch to payload
		execFromMem((void*)0x80800000);
		sleep(3);
	}

	// no homebrew means normal startup
	callMIOS();
	return 0;
}
