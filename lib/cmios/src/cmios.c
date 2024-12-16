#include <string.h>
#include <unistd.h>
#include <gccore.h>

extern int load_dol(void);

void call_mios(void) {
	unsigned long *entry = (unsigned long *) 0x800037fc;

	/* Restore patched entry point. */
	*entry = 0x81300200;
	DCFlushRange(entry, 32);
	ICInvalidateRange(entry, 32);

	/* Simulate boot. */
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
	char *magic = (char *) 0x807FFFE0;

	if (strncmp(magic, "gchomebrew dol", 32) != 0) {
		/* No homebrew means normal startup. */
		call_mios();
	}

	/* Overwrite magic value, so GC disc will work after reset. */
	*magic = 0;
	DCFlushRange(magic, 32);
	ICInvalidateRange(magic, 32);

	load_dol();

	sleep(3);
	call_mios();
	return 0;
}
