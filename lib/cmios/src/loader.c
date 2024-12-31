#include <string.h>
#include <gccore.h>
#include <ogc/machine/processor.h>

typedef struct _dolheader {
	u32 text_data_pos[18]; // text[7] + data[11]
	u32 text_data_start[18];
	u32 text_data_size[18];
	u32 bss_start;
	u32 bss_size;
	u32 entry_point;
} dolheader;

// loads the data of the dol at *dolstart into current execution
// this code was contributed by shagkur of the devkitpro team, thx!
static u32 loadFromMem(void *dolstart) {
	u32 i;
	dolheader *dolfile;

	if (dolstart) {
		dolfile = (dolheader *) dolstart;
		// set bss to 0
		DCInvalidateRange((void *) dolfile->bss_start, dolfile->bss_size);
		memset ((void *) dolfile->bss_start, 0, dolfile->bss_size);
		DCFlushRange((void *) dolfile->bss_start, dolfile->bss_size);
		ICInvalidateRange((void *) dolfile->bss_start, dolfile->bss_size);
		// copy text + data
		for (i = 0; i < 18; i++) {
			if (!dolfile->text_data_size[i] || dolfile->text_data_start[i] < 0x100) { continue; }
			DCInvalidateRange((void *) dolfile->text_data_start[i], dolfile->text_data_size[i]);
			memcpy ((void *) dolfile->text_data_start[i], dolstart+dolfile->text_data_pos[i], dolfile->text_data_size[i]);
			DCFlushRange((void *) dolfile->text_data_start[i], dolfile->text_data_size[i]);
			ICInvalidateRange((void *) dolfile->text_data_start[i], dolfile->text_data_size[i]);
		}
		return dolfile->entry_point;
	}
	return 0;
}

extern void __exception_closeall(void);

// loads dol from memory buffer into execution memory, and executes it
int execFromMem(void* data) {
	void (*entry_point)() = (void(*)())loadFromMem(data);
	unsigned long level = 0;
	
	_CPU_ISR_Disable(level);
	__exception_closeall();
	entry_point();
	return 3;
}
