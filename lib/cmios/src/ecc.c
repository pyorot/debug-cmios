#include "cmios.h"

extern CMIOSTag* tag;

void correctErrors() {
	int corr = 0;
	u32* p[5];
	for (int i=0; i<5; i++) { p[i] = (u32*)tag->payloads[i]; }
	printf(">> %p %p %p %p %p %d\n", p[0], p[1], p[2], p[3], p[4], tag->size);

	for (int i=0; i<tag->size/4; i++) {
		if (*(p[0]+i) != *(p[2]+i) && *(p[2]+i) == *(p[4]+i)) {
			printf("-- [0<2+4] %6x: %08x -> %08x\n", 4*i, *(p[0]+i), *(p[2]+i));
			*(p[0]+i) = *(p[2]+i); 
			corr++; usleep(100000);
		} else
		if (*(p[0]+i) != *(p[1]+i) && *(p[1]+i) == *(p[3]+i)) {
			printf("-- [0<1+3] %6x: %08x -> %08x\n", 4*i, *(p[0]+i), *(p[1]+i));
			*(p[0]+i) = *(p[1]+i);
			corr++; usleep(100000);
		} else
		if (*(p[0]+i) != *(p[1]+i) && *(p[1]+i) == *(p[2]+i)) {
			printf("-- [0<1+2] %6x: %08x -> %08x\n", 4*i, *(p[0]+i), *(p[1]+i));
			*(p[0]+i) = *(p[1]+i);
			corr++; usleep(100000);
		}
	}
	DCFlushRange(p[0], tag->size);
	ICInvalidateRange(p[0], tag->size);	
	printf(">> corrections: %d\n", corr);
}
