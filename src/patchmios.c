/* Copyright (C) 2008 Mega Man */
/* Copyright (C) 2008 WiiGator */
/* Copyright (C) 2010 WiiPower */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <ctype.h>
#include <unistd.h>

#include "elf.h"
#include "patchmios.h"
#include "dolloader_bin.h"

#define round_up(x,n) (-(-(x) & -(n)))

/** Address for entry point. */
#define ENTRY_PATCH_ADDR 0x37fc

/** Binary is loaded to this address. */
#define BIN_PATCH_ADDR 0x81000000

/** Header for Wii ARM binaries. */
typedef struct {
	/** Size of this header. */
	uint32_t headerSize;
	/** Offset to ELF file. */
	uint32_t offset;
	/** Size of ELF file. */
	uint32_t size;
	/** Padded with zeroes. */
	uint32_t resevered;
} arm_binary_header_t;


/**
 * Copy program sections of ELF file to new ELF file;
 * @param buffer Pointer to ELF file.
 */
int copy_sections(uint8_t *buffer, uint8_t *out)
{
	Elf32_Ehdr_t *file_header;
	int pos = 0;
	int i;
	uint32_t outPos = 0;
	uint32_t *entry = NULL;

	/* 0x1000 should be enough to copy ELF header. */
	memcpy(out, buffer, 0x1000);

	/* Use output (copied) elf header. */
	file_header = (Elf32_Ehdr_t *) &out[pos];
	pos += sizeof(Elf32_Ehdr_t);
	if (file_header->magic != ELFMAGIC) {
		//debug_printf("Magic 0x%08x is wrong.\n", file_header->magic);
		return -2;
	}

	/* Add section for loader. */
	file_header->phnum++;

	outPos = pos + file_header->phnum * sizeof(Elf32_Phdr_t);
	//debug_printf("data start = 0x%02x\n", outPos);
	
	for (i = 0; i < file_header->phnum; i++)
	{
		Elf32_Phdr_t *program_header;
		program_header = (Elf32_Phdr_t *) &out[pos];
		pos += sizeof(Elf32_Phdr_t);

		if ((i + 1) == file_header->phnum) {
			program_header->type = PT_LOAD;
			program_header->paddr = BIN_PATCH_ADDR & 0x0FFFFFFF;
			program_header->vaddr = program_header->paddr;
			program_header->filesz = dolloader_bin_size;
			program_header->memsz = program_header->filesz;
			program_header->flags = 0x00F00005;
			program_header->align = 1;
		}
		if ( (program_header->type == PT_LOAD)
			&& (program_header->memsz != 0) )
		{
			unsigned char *src;
			unsigned char *dst;

			// Copy to physical address which can be accessed by loader.
			src = buffer + program_header->offset;

			if (program_header->filesz != 0)
			{
				dst = out + outPos;
				if (program_header->offset != sizeof(Elf32_Ehdr_t)) {
					program_header->offset = outPos;
				} else {
					program_header->filesz = file_header->phnum * sizeof(Elf32_Phdr_t);
					program_header->memsz = program_header->filesz;
				}
				if ((i + 1) == file_header->phnum) {
					//debug_printf("Patching MIOS with binary data.\n");
					memcpy(dst, dolloader_bin, dolloader_bin_size);
				} else {
					memcpy(dst, src, program_header->filesz);
				}
				//debug_printf("VAddr: 0x%08x PAddr: 0x%08x Offset 0x%08x Size 0x%08x\n",
				//	program_header->vaddr,
				//	program_header->paddr,
				//	program_header->offset,
				//	program_header->filesz);
				if (program_header->offset != sizeof(Elf32_Ehdr_t)) {
					outPos += program_header->filesz;
				}
				if ((program_header->paddr <= ENTRY_PATCH_ADDR) && ((program_header->paddr + program_header->filesz) > ENTRY_PATCH_ADDR)) {
					entry = (uint32_t *) (out + program_header->offset - program_header->paddr + ENTRY_PATCH_ADDR);
					if (*entry == 0x81300200) {
						/* Patch entry address into startup code at 0x80003400. */
						*entry = BIN_PATCH_ADDR;
						//debug_printf("Patched entry point.\n");
					} else {
						//debug_printf("Entry point patch failed.\n");
						return -13;
					}
				}
			}
		}
	}
	if (entry != NULL) {
		return 0;
	} else {
		//debug_printf("No entry point found.\n");
		return -3;
	}
}

/**
 * Calculate required memory space.
 * @param buffer Pointer to ELF file.
 */
uint32_t calculate_new_size(uint8_t *buffer)
{
	Elf32_Ehdr_t *file_header;
	int pos = 0;
	int i;
	uint32_t maxSize = 0;
	uint32_t patchSize = 0;
	uint32_t endAddr = 0;

	file_header = (Elf32_Ehdr_t *) &buffer[pos];
	pos += sizeof(Elf32_Ehdr_t);
	if (file_header->magic != ELFMAGIC) {
		//debug_printf("Magic 0x%08x is wrong.\n", file_header->magic);
		return 0;
	}

	for (i = 0; i < file_header->phnum; i++)
	{
		Elf32_Phdr_t *program_header;

		program_header = (Elf32_Phdr_t *) &buffer[pos];
		pos += sizeof(Elf32_Phdr_t);
		if ( (program_header->type == PT_LOAD)
			&& (program_header->memsz != 0) )
		{
			unsigned char *src;

			src = buffer + program_header->offset;

			if (program_header->filesz != 0)
			{
				uint32_t size;

				//debug_printf("VAddr: 0x%08x PAddr: 0x%08x Offset 0x%08x Size 0x%08x\n",
				//	program_header->vaddr,
				//	program_header->paddr,
				//	program_header->offset,
				//	program_header->filesz);
				size = program_header->offset + program_header->filesz;

				if (size > maxSize) {
					maxSize = size;
				}
				if ((program_header->paddr > 0x00010000) && (program_header->paddr < 0x02000000)) {
					if (patchSize == 0) {
						patchSize = program_header->filesz;
						endAddr = program_header->paddr + program_header->filesz;
					}
				}
			}
		}
	}
	if (patchSize == 0) {
		//debug_printf("Patching not possible.\n");
		return 0;
	}
	/* Real calculation after getting all information .*/
	return maxSize + dolloader_bin_size + sizeof(Elf32_Phdr_t);
}

int patchMIOS(uint8_t **buffer)
{
	uint8_t *inbuf = NULL;
	uint8_t *outbuf = NULL;
	arm_binary_header_t *arm_header = NULL;
	uint8_t *inElf = NULL;
	uint8_t *outElf = NULL;
	uint32_t outElfSize;
	uint32_t content_size;

	inbuf = *buffer;
	arm_header = (arm_binary_header_t *) inbuf;

	//debug_printf("patchMIOS called\n");

	if (arm_header->headerSize != sizeof(arm_binary_header_t)) {
		fprintf(stderr, "Error: File is not a Wii ARM binary.\n");
		return -4;
	}

	inElf = inbuf + arm_header->headerSize + arm_header->offset;
	outElfSize = calculate_new_size(inElf);
	if (outElfSize <= 0) {
		//debug_printf("Patching failed\n");
		return -32;
	}
	content_size = round_up((outElfSize + arm_header->headerSize + arm_header->offset), 0x40);
	outbuf = memalign(32, content_size);
	if (outbuf == NULL) {
		//debug_printf("Out of memory\n");
		return -33;
	}
	/* Set default to 0. */
	memset(outbuf, 0, content_size);
	memcpy(outbuf, inbuf, arm_header->headerSize + arm_header->offset);
	arm_header = (arm_binary_header_t *) outbuf;
	arm_header->size = outElfSize;
	outElf = outbuf + arm_header->headerSize + arm_header->offset;
	//debug_printf("\n");
	if (copy_sections(inElf, outElf) < 0) {
		//debug_printf("Failed to patch ELF.\n");
		return -39;
	}

	*buffer = outbuf;
	free(inbuf);
	if ((arm_header->headerSize + arm_header->offset + arm_header->size) > content_size) {
		//debug_printf("content size %d\n", arm_header->headerSize + arm_header->offset + arm_header->size);
		//debug_printf("content size expected %d\n", content_size);
		//debug_printf("Wrong content size calculation.\n");
		return -35;
	}
	return arm_header->headerSize + arm_header->offset + arm_header->size;
}
