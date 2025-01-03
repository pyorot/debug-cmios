// beware: i copied this code from a branch of Wii Swiss Booter w/o testing it so it may not compile right-away
// that (tested) branch is at: https://github.com/pyorot/wii-swiss-booter/tree/cmios-experiment

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <gccore.h>
#include <fat.h>

#include "sha1.h"
#include "console.h"

#define INJECTED	// inject swiss.dol binary (placed in data folder) into cmios

#define BC      0x0000000100000100ULL   // title ID of GameCube bootloader

char* errStr = "";
int ret = 0;
int fail();

typedef struct {
	u32 magic;
	u32 size;
	u8 hash[20];
	u8* payloads[9];
} __attribute__((packed)) CMIOSTag;

static void* cmiosTagLoc = (void*)0x80001800;
static CMIOSTag cmiosTag = {
	.magic = 0x1d0110ad,
	.payloads = {(u8*)0x80300000, (u8*)0x80500020, (u8*)0x80700060, (u8*)0x80a00080, (u8*)0x80c00040},
};

int EXECGCDOL_LoadFile(FILE* fp) {
	// set filesize
	fseek(fp, 0, SEEK_END);
	cmiosTag.size = ftell(fp);
	printf("filesize:    %d\n", cmiosTag.size);

	// read full Dol file into memory at cmiosTag.payloads
	for (int i=0; i<sizeof(cmiosTag.payloads)/sizeof(cmiosTag.payloads[0]); i++) {
		if ((u32)cmiosTag.payloads[i] < 0x80000000 || (u32)cmiosTag.payloads[i] >= 0x81800000) { continue; }
		printf("swiss.dol  : %p -- %p\n", cmiosTag.payloads[i], cmiosTag.payloads[i]+cmiosTag.size);
		fseek(fp, 0, SEEK_SET);
		fread(cmiosTag.payloads[i], cmiosTag.size, 1, fp);
		DCFlushRange(cmiosTag.payloads[i], cmiosTag.size);
		ICInvalidateRange(cmiosTag.payloads[i], cmiosTag.size);
	}

	// hash
	printf("hash");
	for (int i=sizeof(cmiosTag.payloads)/sizeof(cmiosTag.payloads[0])-1; i>=0; i--) {
		if ((u32)cmiosTag.payloads[i] < 0x80000000 || (u32)cmiosTag.payloads[i] >= 0x81800000) { continue; }
		SHA1(cmiosTag.payloads[i], cmiosTag.size, cmiosTag.hash);
		printf(" %02x%02x", cmiosTag.hash[0], cmiosTag.hash[1]);
	}
	printf("\n");
	
	// communicate with cMIOS
	memcpy(cmiosTagLoc, &cmiosTag, sizeof(cmiosTag));
	DCFlushRange(cmiosTagLoc, sizeof(cmiosTag));
	ICInvalidateRange(cmiosTagLoc, sizeof(cmiosTag));
	/*// communicate with cMIOS (legacy)
	strcpy((char*)0x807FFFE0, "gchomebrew dol");
	DCFlushRange((char*)0x807FFFE0, 32);
	ICInvalidateRange((char*)0x807FFFE0, 32);*/
	return 0;
}

void EXECGCDOL_Signal(u32 signal) {
	*(u32*)0x80001800 = signal;
	DCFlushRange((void*)0x80001800, 4);
	ICInvalidateRange((void*)0x80001800, 4);    
}

int go() {
	videoShow(true);
	printf("== ExecGCDol ==\n\n");

#ifdef INJECTED
	EXECGCDOL_Signal(0x50155d01);
#else
	if (!fatInitDefault()) { errStr = "(fatInitDefault)"; return fail(); }; 
	FILE* fp = fopen("usb:/apps/Swiss/swiss.dol", "rb");
	if (!fp) { errStr = strerror(errno); return fail(); };
	ret = EXECGCDOL_LoadFile(fp);
	fclose(fp);
	if (ret != 0) { errStr = "(EXECGCDOL_LoadFile)"; return fail(); };
#endif

	sleep(1);
	while (padScanOnNextFrame()) { // while controller connected, stall if A held 
		if ((~PAD_ButtonsHeld(0)) & PAD_BUTTON_A) { break; }
	}

	printf(CON_CYAN("Launching...\n")); // dunno if better to use WII_LaunchTitle
	static tikview view ATTRIBUTE_ALIGN(32); // requires alignment, so static
	ret = ES_GetTicketViews(BC, &view, 1);
	if (ret != 0) {	errStr = "(ES_GetTicketViews)"; return fail(); }
	ret = ES_LaunchTitle(BC, &view);
	return ret;
}

int fail() {
	videoShow(true);
	printf(CON_RED("ERROR | %s [%d]\n"), errStr, ret);
	printf("Press A to retry or B to exit.\n");
	while (padScanOnNextFrame()) { // while controller connected, await input 
		if (PAD_ButtonsDown(0) & PAD_BUTTON_A) { consoleClear(); usleep(300000); return go(); }
		if (PAD_ButtonsDown(0) & PAD_BUTTON_B) { break; }
	}
	printf("Exiting...\n");
	return 0;
}

int main(int argc, char* argv[]) {
	consoleInit();
	return go();
}
