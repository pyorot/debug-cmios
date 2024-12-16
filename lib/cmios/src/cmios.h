#pragma once

#include <stdio.h>
#include <unistd.h>
#include <gccore.h>

typedef struct {
	u32 magic;
	u32 size;
	u8 hash[20];
	u8* payloads[9];
} __attribute__((packed)) CMIOSTag;

int execFromMem(void* data);
void correctErrors(void);
bool verify(void);
