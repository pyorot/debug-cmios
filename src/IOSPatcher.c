#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <network.h>

#include "IOSPatcher.h"
#include "rijndael.h"
#include "sha1.h"
#include "tools.h"
#include "http.h"
#include "patchmios.h"
#include "cert_sys.h"

#define round_up(x,n)	(-(-(x) & -(n)))
#define TITLE_UPPER(x)		( (u32)((x) >> 32) )
#define TITLE_LOWER(x)		((u32)(x))

u8 commonkey[16] = { 0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7 };

s32 Wad_Read_into_memory(char *filename, IOS **ios, u32 iosnr, u32 revision);


int patch_dvdAccess(u8 *buf, u32 size)
// Clear bit 0x00200000 in register 0x0D800180 to enable DVD-R Support. Just like syscall_50(0) would do on IOS.
{
	u32 match_count = 0;
	u8 dvdAccessOrig[] = { 0x23, 0x80, 0x03, 0x9b, 0x43, 0x1a };
	u8 dvdAccessPatch[] = { 0x23, 0x80, 0x03, 0x9b, 0x43, 0x9a };
	u32 i;
	
	for(i = 0; i < size - 4; i++)
	{
		if (!memcmp(buf + i, dvdAccessOrig, sizeof(dvdAccessOrig)))
		{
			memcpy(buf + i, dvdAccessPatch, sizeof(dvdAccessPatch));
			i += sizeof(dvdAccessPatch);
			match_count++;
			continue;
		}
	}
	return match_count;
}
/*
int patch_mem2Access(u8 *buf, u32 size)
{
	u32 match_count = 0;
	u8 mem2AccessOrig[] = { 0x0d, 0x8b, 0x42, 0x0a };
	u8 mem2AccessPatch[] = { 0x01, 0x7b, 0x42, 0x0a };
	u32 i;
	
	for(i = 0; i < size - 4; i++)
	{
		if (!memcmp(buf + i, mem2AccessOrig, sizeof(mem2AccessOrig)))
		{
			memcpy(buf + i, mem2AccessPatch, sizeof(mem2AccessPatch));
			i += sizeof(mem2AccessPatch);
			match_count++;
			continue;
		}
	}
	return match_count;
}
*/

int patch_Action_Replay(u8 *buf, u32 size)
// Clear bit 0x00200000 in register 0x0D800180 to enable DVD-R Support. Just like syscall_50(0) would do on IOS.
{
	u32 match_count = 0;
	u8 actionreplayold[] = { 0x47, 0x4e, 0x48, 0x45 };
	u8 actionreplaynew[] = { 0x58, 0x58, 0x58, 0x58 };
	u32 i;
	
	for(i = 0; i < size - 4; i++)
	{
		if (!memcmp(buf + i, actionreplayold, sizeof(actionreplayold)))
		{
			memcpy(buf + i, actionreplaynew, sizeof(actionreplaynew));
			i += sizeof(actionreplaynew);
			match_count++;
			continue;
		}
	}
	return match_count;
}

void get_title_key(signed_blob *s_tik, u8 *key) 
{
	static u8 iv[16] ATTRIBUTE_ALIGN(0x20);
	static u8 keyin[16] ATTRIBUTE_ALIGN(0x20);
	static u8 keyout[16] ATTRIBUTE_ALIGN(0x20);

	const tik *p_tik;
	p_tik = (tik*)SIGNATURE_PAYLOAD(s_tik);
	u8 *enc_key = (u8 *)&p_tik->cipher_title_key;
	memcpy(keyin, enc_key, sizeof keyin);
	memset(keyout, 0, sizeof keyout);
	memset(iv, 0, sizeof iv);
	memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);
 
	aes_set_key(commonkey);
	aes_decrypt(iv, keyin, keyout, sizeof keyin);

	memcpy(key, keyout, sizeof keyout);
}

void change_ticket_title_id(signed_blob *s_tik, u32 titleid1, u32 titleid2) 
{
	static u8 iv[16] ATTRIBUTE_ALIGN(0x20);
	static u8 keyin[16] ATTRIBUTE_ALIGN(0x20);
	static u8 keyout[16] ATTRIBUTE_ALIGN(0x20);

	tik *p_tik;
	p_tik = (tik*)SIGNATURE_PAYLOAD(s_tik);
	u8 *enc_key = (u8 *)&p_tik->cipher_title_key;
	memcpy(keyin, enc_key, sizeof keyin);
	memset(keyout, 0, sizeof keyout);
	memset(iv, 0, sizeof iv);
	memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);

	aes_set_key(commonkey);
	aes_decrypt(iv, keyin, keyout, sizeof keyin);
	p_tik->titleid = (u64)titleid1 << 32 | (u64)titleid2;
	memset(iv, 0, sizeof iv);
	memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);
	
	aes_encrypt(iv, keyout, keyin, sizeof keyout);
	memcpy(enc_key, keyin, sizeof keyin);
}

void change_tmd_title_id(signed_blob *s_tmd, u32 titleid1, u32 titleid2) 
{
	tmd *p_tmd;
	u64 title_id = titleid1;
	title_id <<= 32;
	title_id |= titleid2;
	p_tmd = (tmd*)SIGNATURE_PAYLOAD(s_tmd);
	p_tmd->title_id = title_id;
}


void zero_sig(signed_blob *sig) 
{
	u8 *sig_ptr = (u8 *)sig;
	memset(sig_ptr + 4, 0, SIGNATURE_SIZE(sig)-4);
}

s32 brute_tmd(tmd *p_tmd) 
{
	u16 fill;
	for(fill=0; fill<65535; fill++) 
	{
		p_tmd->fill3=fill;
		sha1 hash;
		SHA1((u8 *)p_tmd, TMD_SIZE(p_tmd), hash);;
		  
		if (hash[0]==0) 
		{
			return 0;
		}
	}
	return -1;
}

s32 brute_tik(tik *p_tik) 
{
	u16 fill;
	for(fill=0; fill<65535; fill++) 
	{
		p_tik->padding=fill;
		sha1 hash;
		SHA1((u8 *)p_tik, sizeof(tik), hash);

		if (hash[0]==0)
		{
			return 0;
		}
	}
	return -1;
}
    
void forge_tmd(signed_blob *s_tmd) 
{
	zero_sig(s_tmd);
	brute_tmd(SIGNATURE_PAYLOAD(s_tmd));
}

void forge_tik(signed_blob *s_tik) 
{
	zero_sig(s_tik);
	brute_tik(SIGNATURE_PAYLOAD(s_tik));
}


void display_tag(u8 *buf) 
{
	printf("Firmware version: %s      Builder: %s\n", buf, buf+0x30);
}

void display_ios_tags(u8 *buf, u32 size) 
{
	u32 i;
	char *ios_version_tag = "$IOSVersion:";

	if (size == 64) 
	{
		display_tag(buf);
		return;
	}

	for (i=0; i<(size-64); i++) 
	{
		if (!strncmp((char *)buf+i, ios_version_tag, 10)) 
		{
			char version_buf[128], *date;
			while (buf[i+strlen(ios_version_tag)] == ' ') i++; // skip spaces
			strlcpy(version_buf, (char *)buf + i + strlen(ios_version_tag), sizeof version_buf);
			date = version_buf;
			strsep(&date, "$");
			date = version_buf;
			strsep(&date, ":");
			printf("%s (%s)\n", version_buf, date);
			i += 64;
		}
	}
}


void decrypt_buffer(u16 index, u8 *source, u8 *dest, u32 len) 
{
	static u8 iv[16];
	memset(iv, 0, 16);
	memcpy(iv, &index, 2);
	aes_decrypt(iv, source, dest, len);
}

void encrypt_buffer(u16 index, u8 *source, u8 *dest, u32 len) 
{
	static u8 iv[16];
	memset(iv, 0, 16);
	memcpy(iv, &index, 2);
	aes_encrypt(iv, source, dest, len);
}

void decrypt_IOS(IOS *ios)
{
	u8 key[16];
	get_title_key(ios->ticket, key);
	aes_set_key(key);
	
	int i;
	for (i = 0; i < ios->content_count; i++)
	{
		decrypt_buffer(i, ios->encrypted_buffer[i], ios->decrypted_buffer[i], ios->buffer_size[i]);
	}
}

void encrypt_IOS(IOS *ios)
{
	u8 key[16];
	get_title_key(ios->ticket, key);
	aes_set_key(key);
	
	int i;
	for (i = 0; i < ios->content_count; i++)
	{
		encrypt_buffer(i, ios->decrypted_buffer[i], ios->encrypted_buffer[i], ios->buffer_size[i]);
	}
}

void display_tags(IOS *ios)
{
	int i;
	for (i = 0; i < ios->content_count; i++)
	{
		printf("Content %2u:  ", i);

		display_ios_tags(ios->decrypted_buffer[i], ios->buffer_size[i]);
	}
}

s32 Init_IOS(IOS **ios)
{
	if (ios == NULL)
		return -1;
		
	*ios = memalign(32, sizeof(IOS));
	if (*ios == NULL)
		return -1;
	
	(*ios)->content_count = 0;

	(*ios)->certs = NULL;
	(*ios)->certs_size = 0;
	(*ios)->ticket = NULL;
	(*ios)->ticket_size = 0;
	(*ios)->tmd = NULL;
	(*ios)->tmd_size = 0;
	(*ios)->crl = NULL;
	(*ios)->crl_size = 0;
	
	(*ios)->encrypted_buffer = NULL;
	(*ios)->decrypted_buffer = NULL;
	(*ios)->buffer_size = NULL;
	
	return 0;
}

void free_IOS(IOS **ios)
{
	if (ios && *ios)
	{
		if ((*ios)->certs) free((*ios)->certs);
		if ((*ios)->ticket) free((*ios)->ticket);
		if ((*ios)->tmd) free((*ios)->tmd);
		if ((*ios)->crl) free((*ios)->crl);
		
		int i;
		for (i = 0; i < (*ios)->content_count; i++)
		{
			if ((*ios)->encrypted_buffer && (*ios)->encrypted_buffer[i]) free((*ios)->encrypted_buffer[i]);
			if ((*ios)->decrypted_buffer && (*ios)->decrypted_buffer[i]) free((*ios)->decrypted_buffer[i]);
		}
		
		if ((*ios)->encrypted_buffer) free((*ios)->encrypted_buffer);
		if ((*ios)->decrypted_buffer) free((*ios)->decrypted_buffer);
		if ((*ios)->buffer_size) free((*ios)->buffer_size);
		free(*ios);
	}	
}

s32 set_content_count(IOS *ios, u32 count)
{
	int i;
	if (ios->content_count > 0)
	{
		for (i = 0; i < ios->content_count; i++)
		{
			if (ios->encrypted_buffer && ios->encrypted_buffer[i]) free(ios->encrypted_buffer[i]);
			if (ios->decrypted_buffer && ios->decrypted_buffer[i]) free(ios->decrypted_buffer[i]);
		}
		
		if (ios->encrypted_buffer) free(ios->encrypted_buffer);
		if (ios->decrypted_buffer) free(ios->decrypted_buffer);
		if (ios->buffer_size) free(ios->buffer_size);
	}
	
	ios->content_count = count;
	if (count > 0)
	{
		ios->encrypted_buffer = memalign(32, 4*count);
		ios->decrypted_buffer = memalign(32, 4*count);
		ios->buffer_size = memalign(32, 4*count);
		
		for (i = 0; i < count; i++) 
		{
			if (ios->encrypted_buffer) ios->encrypted_buffer[i] = NULL;
			if (ios->decrypted_buffer) ios->decrypted_buffer[i] = NULL;
		}

		if (!ios->encrypted_buffer || !ios->decrypted_buffer || !ios->buffer_size)
		{
			return -1;
		}
	}
	return 0;
}

s32 install_IOS(IOS *ios, bool skipticket)
{
	int ret;
	int cfd;

	if (!skipticket)
	{
		ret = ES_AddTicket(ios->ticket, ios->ticket_size, ios->certs, ios->certs_size, ios->crl, ios->crl_size);
		if (ret < 0)
		{
			printf("ES_AddTicket returned: %d\n", ret);
			ES_AddTitleCancel();
			return ret;
		}
	}
	printf(".");
	
	ret = ES_AddTitleStart(ios->tmd, ios->tmd_size, ios->certs, ios->certs_size, ios->crl, ios->crl_size);
	if (ret < 0)
	{
		printf("\nES_AddTitleStart returned: %d\n", ret);
		ES_AddTitleCancel();
		return ret;
	}
	printf(".");
	
	tmd *tmd_data  = (tmd *)SIGNATURE_PAYLOAD(ios->tmd);

	int i;
	for (i = 0; i < ios->content_count; i++)
	{
		tmd_content *content = &tmd_data->contents[i];

		cfd = ES_AddContentStart(tmd_data->title_id, content->cid);
		if (cfd < 0)
		{
			printf("\nES_AddContentStart for content #%u cid %u returned: %d\n", i, content->cid, cfd);
			ES_AddTitleCancel();
			return cfd;
		}
		
		ret = ES_AddContentData(cfd, ios->encrypted_buffer[i], ios->buffer_size[i]);
		if (ret < 0)
		{
			printf("\nES_AddContentData for content #%u cid %u returned: %d\n", i, content->cid, ret);
			ES_AddTitleCancel();
			return ret;
		}
		
		ret = ES_AddContentFinish(cfd);
		if (ret < 0)
		{
			printf("\nES_AddContentFinish for content #%u cid %u returned: %d\n", i, content->cid, ret);
			ES_AddTitleCancel();
			return ret;
		}
		printf(".");
	}
	
	ret = ES_AddTitleFinish();
	if (ret < 0)
	{
		printf("\nES_AddTitleFinish returned: %d\n", ret);
		ES_AddTitleCancel();
		return ret;
	}
	printf(".\n");
	
	return 0;
}

int get_nus_object(u32 titleid1, u32 titleid2, char *content, u8 **outbuf, u32 *outlen) 
{
	static char buf[128];
	int retval;
	u32 http_status;

	snprintf(buf, 128, "http://nus.cdn.shop.wii.com/ccs/download/%08x%08x/%s", titleid1, titleid2, content);
	
	retval = http_request(buf, 1 << 31);
	if (!retval) 
	{
		printf("Error making http request\n");
		return -1;
	}

	retval = http_get_result(&http_status, outbuf, outlen); 
	
	if (((int)*outbuf & 0xF0000000) == 0xF0000000) 
	{
		return (int) *outbuf;
	}
	return 0;
}


s32 GetCerts(signed_blob** Certs, u32* Length)
{
	if (cert_sys_size != 2560)
	{
		return -1;
	}
	*Certs = memalign(32, 2560);
	if (*Certs == NULL)
	{
		printf("Out of memory\n");
		return -1;	
	}
	memcpy(*Certs, cert_sys, cert_sys_size);
	*Length = 2560;

	return 0;
}


s32 Download_IOS(IOS **ios, u32 iosnr, u32 revision)
{
	s32 ret;

	ret = Init_IOS(ios);
	if (ret < 0)
	{
		printf("Out of memory\n");
		goto err;
	}

	tmd *tmd_data  = NULL;
	u32 cnt;
	static bool network_initialized = false;
	char buf[32];
	
	if (!network_initialized)
	{
		printf("Initializing network...");
		while (1) 
		{
			ret = net_init ();
			if (ret < 0) 
			{
				//if (ret != -EAGAIN) 
				if (ret != -11) 
				{
					printf ("net_init failed: %d\n", ret);
					goto err;
				}
			}
			if (!ret) break;
			usleep(100000);
			printf(".");
		}
		printf("done\n");
		network_initialized = true;
	}

	printf("Loading certs...\n");
	ret = GetCerts(&((*ios)->certs), &((*ios)->certs_size));
	if (ret < 0)
	{
		printf ("Loading certs from nand failed, ret = %d\n", ret);
		goto err;	
	}

	if ((*ios)->certs == NULL || (*ios)->certs_size == 0)
	{
		printf("certs error\n");
		ret = -1;
		goto err;		
	}

	if (!IS_VALID_SIGNATURE((*ios)->certs))
	{
		printf("Error: Bad certs signature!\n");
		ret = -1;
		goto err;
	}
	
	printf("Loading TMD...\n");
	sprintf(buf, "tmd.%u", revision);
	u8 *tmd_buffer = NULL;
	ret = get_nus_object(1, iosnr, buf, &tmd_buffer, &((*ios)->tmd_size));
	if (ret < 0)
	{
		printf("Loading tmd failed, ret = %u\n", ret);
		goto err;	
	}

	if (tmd_buffer == NULL || (*ios)->tmd_size == 0)
	{
		printf("TMD error\n");
		ret = -1;
		goto err;		
	}
	
	(*ios)->tmd_size = SIGNED_TMD_SIZE((signed_blob *)tmd_buffer);
 	(*ios)->tmd = memalign(32, (*ios)->tmd_size);
	if ((*ios)->tmd == NULL)
	{
		printf("Out of memory\n");
		ret = -1;
		goto err;		
	}
	memcpy((*ios)->tmd, tmd_buffer, (*ios)->tmd_size);
	free(tmd_buffer);
	
	if (!IS_VALID_SIGNATURE((*ios)->tmd))
	{
		printf("Error: Bad TMD signature!\n");
		ret = -1;
		goto err;
	}

	printf("Loading ticket...\n");
	u8 *ticket_buffer = NULL;
	ret = get_nus_object(1, iosnr, "cetk", &ticket_buffer, &((*ios)->ticket_size));
	if (ret < 0)
	{
		printf("Loading ticket failed, ret = %u\n", ret);
		goto err;	
	}

	if (ticket_buffer == NULL || (*ios)->ticket_size == 0)
	{
		printf("ticket error\n");
		ret = -1;
		goto err;		
	}

	(*ios)->ticket_size = SIGNED_TIK_SIZE((signed_blob *)ticket_buffer);
 	(*ios)->ticket = memalign(32, (*ios)->ticket_size);
	if ((*ios)->ticket == NULL)
	{
		printf("Out of memory\n");
		ret = -1;
		goto err;		
	}
	memcpy((*ios)->ticket, ticket_buffer, (*ios)->ticket_size);
	free(ticket_buffer);
	
	if(!IS_VALID_SIGNATURE((*ios)->ticket))
	{
		printf("Error: Bad ticket signature!\n");
		ret = -1;
		goto err;
	}

	/* Get TMD info */
	tmd_data = (tmd *)SIGNATURE_PAYLOAD((*ios)->tmd);

	printf("Checking titleid and revision...\n");
	if (TITLE_UPPER(tmd_data->title_id) != 1 || TITLE_LOWER(tmd_data->title_id) != iosnr)
	{
		printf("IOS has titleid: %08x%08x but expected was: %08x%08x\n", TITLE_UPPER(tmd_data->title_id), TITLE_LOWER(tmd_data->title_id), 1, iosnr);
		ret = -1;
		goto err;
	}

	if (tmd_data->title_version != revision)
	{
		printf("IOS has revision: %u but expected was: %u\n", tmd_data->title_version, revision);
		ret = -1;
		goto err;
	}

	ret = set_content_count(*ios, tmd_data->num_contents);
	if (ret < 0)
	{
		printf("Out of memory\n");
		goto err;
	}

	printf("Loading contents");
	for (cnt = 0; cnt < tmd_data->num_contents; cnt++) 
	{
		printf(".");
		tmd_content *content = &tmd_data->contents[cnt];

		/* Encrypted content size */
		(*ios)->buffer_size[cnt] = round_up((u32)content->size, 64);

		sprintf(buf, "%08x", content->cid);
   
		ret = get_nus_object(1, iosnr, buf, &((*ios)->encrypted_buffer[cnt]), &((*ios)->buffer_size[cnt]));

		if ((*ios)->buffer_size[cnt] % 16) 
		{
			printf("Content %u size is not a multiple of 16\n", cnt);
			ret = -1;
			goto err;
		}

   		if ((*ios)->buffer_size[cnt] < (u32)content->size) 
		{
			printf("Content %u size is too small\n", cnt);
			ret = -1;
			goto err;
   		} 

		(*ios)->decrypted_buffer[cnt] = memalign(32, (*ios)->buffer_size[cnt]);
		if (!(*ios)->decrypted_buffer[cnt])
		{
			printf("Out of memory\n");
			ret = -1;
			goto err;
		}

	}
	printf("done\n");
	
	printf("Reading file into memory complete.\n");

	printf("Decrypting MIOS...\n");
	decrypt_IOS(*ios);

	tmd_content *p_cr = TMD_CONTENTS(tmd_data);
	sha1 hash;
	int i;

	printf("Checking hashes...\n");
	for (i=0;i < (*ios)->content_count;i++)
	{
		SHA1((*ios)->decrypted_buffer[i], (u32)p_cr[i].size, hash);
		if (memcmp(p_cr[i].hash, hash, sizeof hash) != 0)
		{
			printf("Wrong hash for content #%u\n", i);
			ret = -1;
			goto err;
		}
	}	

	goto out;

err:
	free_IOS(ios);

out:
	return ret;
}

s32 get_IOS(IOS **ios, u32 iosnr, u32 revision)
{
	char buf[64];
	u32 pressed;
	u32 pressedGC;
	int selection = 0;
	int ret;
	char *optionsstring[4] = { "<Load MIOS from sd card>", "<Load MIOS from usb storage>", "<Download MIOS from NUS>", "<Exit>" };
	
	while (true)
	{
		Con_ClearLine();
		
		set_highlight(true);
		printf(optionsstring[selection]);
		set_highlight(false);
		
		waitforbuttonpress(&pressed, &pressedGC);
		
		if (pressed == WPAD_BUTTON_LEFT || pressedGC == PAD_BUTTON_LEFT)
		{
			if (selection > 0)
			{
				selection--;
			} else
			{
				selection = 3;
			}
		}

		if (pressed == WPAD_BUTTON_RIGHT || pressedGC == PAD_BUTTON_RIGHT)
		{
			if (selection < 3)
			{
				selection++;
			} else
			{
				selection = 0;
			}
		}

		if (pressed == WPAD_BUTTON_A || pressedGC == PAD_BUTTON_A)
		{
			printf("\n");
			if (selection == 0)
			{
				ret = Init_SD();
				if (ret < 0)
				{
					return ret;
				}		
				
				sprintf(buf, "sd:/RVL-mios-v%u.wad", revision);
				ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
				if (ret < 0)
				{
					sprintf(buf, "sd:/RVL-mios-v%u.wad.out.wad", revision);
					ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
				}
				if (ret < 0)
				{
					sprintf(buf, "sd:/MIOSv%u.wad", revision);
					ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
				}
				return ret;
			}
			
			if (selection == 1)
			{
				ret = Init_USB();
				if (ret < 0)
				{
					return ret;
				}		
				
				sprintf(buf, "usb:/RVL-mios-v%u.wad", revision);
				ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
				if (ret < 0)
				{
					sprintf(buf, "usb:/RVL-mios-v%u.wad.out.wad", revision);
					ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
				}
				if (ret < 0)
				{
					sprintf(buf, "usb:/MIOSv%u.wad", revision);
					ret = Wad_Read_into_memory(buf, ios, iosnr, revision);
				}
				return ret;
			}

			if (selection == 2)
			{
				ret = Download_IOS(ios, iosnr, revision);
				return ret;
			}
			
			if (selection == 3)
			{
				return -1;
			}
		}
	}	
}

s32 install_unpatched_MIOS(u32 iosversion, u32 revision)
{
	int ret;
	IOS *ios;
	
	printf("Getting MIOS%u revision %u...\n", iosversion, revision);
	ret = get_IOS(&ios, iosversion, revision);
	if (ret < 0)
	{
		printf("Error reading MIOS into memory\n");
		return ret;
	}
	
	printf("\n");
	printf("Press A to start installation...\n");

	u32 pressed;
	u32 pressedGC;	
	waitforbuttonpress(&pressed, &pressedGC);
	if (pressed != WPAD_BUTTON_A && pressedGC != PAD_BUTTON_A)
	{
		printf("Other button pressed\n");
		free_IOS(&ios);
		return -1;
	}
	
	printf("Installing MIOSv%u...\n", revision);
	ret = install_IOS(ios, false);
	if (ret < 0)
	{
		printf("Error: Could not install MIOS%u Rev %u\n", iosversion, revision);
		free_IOS(&ios);
		return ret;
	}
	printf("done\n");

	free_IOS(&ios);
	return 0;
}


s32 Install_patched_MIOS(u32 iosnr, u32 iosrevision, bool patchhomebrew, u32 newrevision)
{
	int ret;
	if (iosrevision == newrevision && !patchhomebrew)
	{
		ret = install_unpatched_MIOS(iosnr, iosrevision);
		return ret;
	}
	
	IOS *ios;
	int index = -1;
	bool tmd_dirty = false;
	bool tik_dirty = false;
	
	printf("Getting MIOSv%u...\n", iosrevision);
	ret = get_IOS(&ios, iosnr, iosrevision);
	if (ret < 0)
	{
		printf("Error reading MIOS into memory\n");
		return -1;
	}
	
	tmd *p_tmd = (tmd*)SIGNATURE_PAYLOAD(ios->tmd);
	tmd_content *p_cr = TMD_CONTENTS(p_tmd);

	if (patchhomebrew)
	{
		u32 i;
		for (i=0;i<p_tmd->num_contents;i++)
		{
			if (p_cr[i].index == 1)
			{
				index = i;
			}
		}
		if (index == -1)
		{
			free_IOS(&ios);
			printf("Couldn't find module to patch\n");
			return ret;
		}

		u32 patchcount;
		patchcount = patch_dvdAccess((u8 *)(ios->decrypted_buffer[index]), ios->buffer_size[index]);
		if (patchcount == 0)
		{
			printf("Couldn't patch MIOS for DVD-R access\n");			
		} else
		{
			printf("Patched MIOS for DVD-R access(%u times)\n", patchcount);			
		}

		patchcount = patch_Action_Replay((u8 *)(ios->decrypted_buffer[index]), ios->buffer_size[index]);
		if (patchcount == 0)
		{
			printf("Couldn't patch MIOS for Action Replay support\n");			
		} else
		{
			printf("Patched MIOS for Action Replay support(%u times)\n", patchcount);			
		}
/*
		patchcount = patch_mem2Access((u8 *)(ios->decrypted_buffer[index]), ios->buffer_size[index]);
		if (patchcount == 0)
		{
			printf("Couldn't patch MIOS for mem2 access\n");			
		} else
		{
			printf("Patched MIOS for mem2 access(%u times)\n", patchcount);			
		}
*/
		ret = patchMIOS(&(ios->decrypted_buffer[index]));
		if (ret <= 0)
		{
			free_IOS(&ios);
			printf("Patching failed\n");
			return ret;
		}
		printf("Patched MIOS for homebrew support\n");
		
		ret = (ret+31)&(~31);	// Round up content size
		
		p_cr[index].size = ret;
		
		if (ret > ios->buffer_size[index])
		{
			void *temp = memalign(32, ret);
			if (temp == NULL)
			{
				free_IOS(&ios);
				printf("Out of memory\n");
				return -1;
			} else
			{
				free(ios->encrypted_buffer[index]);
				ios->encrypted_buffer[index] = temp;
			}
		}
		ios->buffer_size[index] = ret;

		// Force the patched module to be not shared
		tmd_content *content = &p_tmd->contents[index];
		content->type = 1;

		p_tmd->access_rights = 1;
		printf("Access rights set to 1(why not?)\n");
				
		// Update the content hash inside the tmd
		sha1 hash;
		SHA1(ios->decrypted_buffer[index], (u32)p_cr[index].size, hash);
		memcpy(p_cr[index].hash, hash, sizeof hash);
		tmd_dirty = true;
	}

	if (iosrevision != newrevision)
	{
		p_tmd->title_version = newrevision;
		tmd_dirty = true;
	}

	if (tmd_dirty)
	{
		printf("Trucha signing the tmd...\n");
		forge_tmd(ios->tmd);
	}

	if (tik_dirty)
	{
		printf("Trucha signing the ticket..\n");
		forge_tik(ios->ticket);
	}

	printf("Encrypting MIOS...\n");
	encrypt_IOS(ios);
	
	printf("Preparations complete\n\n");
	printf("Press A to start the install...\n");

	u32 pressed;
	u32 pressedGC;	
	waitforbuttonpress(&pressed, &pressedGC);
	if (pressed != WPAD_BUTTON_A && pressedGC != PAD_BUTTON_A)
	{
		printf("Other button pressed\n");
		free_IOS(&ios);
		return -1;
	}

	printf("Installing...\n");
	ret = install_IOS(ios, false);
	if (ret < 0)
	{
		free_IOS(&ios);
		if (ret == -1017 || ret == -2011)
		{
			printf("You need to use an IOS with trucha bug.\n");
			printf("Up to system menu 4.2, you can get one with Trucha Bug Restorer.\n");
		} else
		if (ret == -1035)
		{
			printf("'Downgrade' failed, use an IOS that ignores the version of titles.\n");
			printf("Up to system menu 4.2, you can get one with Trucha Bug Restorer 1.12.\n");
		}
		
		return -1;
	}
	printf("done\n");
	
	free_IOS(&ios);
	return 0;
}

