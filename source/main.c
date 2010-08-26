#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <malloc.h>

#include "IOSPatcher.h"
#include "wad.h"
#include "tools.h"


// Prevent IOS36 loading at startup
s32 __IOS_LoadStartupIOS()
{
	return 0;
}

s32 __u8Cmp(const void *a, const void *b)
{
	return *(u8 *)a-*(u8 *)b;
}

u8 *get_ioslist(u32 *cnt)
{
	u64 *buf = 0;
	s32 i, res;
	u32 tcnt = 0, icnt;
	u8 *ioses = NULL;
	
	//Get stored IOS versions.
	res = ES_GetNumTitles(&tcnt);
	if(res < 0)
	{
		printf("ES_GetNumTitles: Error! (result = %d)\n", res);
		return 0;
	}
	buf = memalign(32, sizeof(u64) * tcnt);
	res = ES_GetTitles(buf, tcnt);
	if(res < 0)
	{
		printf("ES_GetTitles: Error! (result = %d)\n", res);
		if (buf) free(buf);
		return 0;
	}

	icnt = 0;
	for(i = 0; i < tcnt; i++)
	{
		if(*((u32 *)(&(buf[i]))) == 1 && (u32)buf[i] > 2 && (u32)buf[i] < 0x100)
		{
			icnt++;
			ioses = (u8 *)realloc(ioses, sizeof(u8) * icnt);
			ioses[icnt - 1] = (u8)buf[i];
		}
	}

	ioses = (u8 *)malloc(sizeof(u8) * icnt);
	icnt = 0;
	
	for(i = 0; i < tcnt; i++)
	{
		if(*((u32 *)(&(buf[i]))) == 1 && (u32)buf[i] > 2 && (u32)buf[i] < 0x100)
		{
			icnt++;
			ioses[icnt - 1] = (u8)buf[i];
		}
	}
	free(buf);
	qsort(ioses, icnt, 1, __u8Cmp);

	*cnt = icnt;
	return ioses;
}


int ios_selectionmenu(int default_ios)
{
	u32 pressed;
	u32 pressedGC;
	int selection = 0;
	u32 ioscount;
	u8 *list = get_ioslist(&ioscount);
	
	int i;
	for (i=0;i<ioscount;i++)
	{
		// Default to default_ios if found, else the loaded IOS
		if (list[i] == default_ios)
		{
			selection = i;
			break;
		}
		if (list[i] == IOS_GetVersion())
		{
			selection = i;
		}
	}	
	
	while (true)
	{
		printf("\x1B[%d;%dH",2,0);	// move console cursor to y/x
		printf("Select which IOS to load:       \b\b\b\b\b\b");
		
		set_highlight(true);
		printf("IOS%u\n", list[selection]);
		set_highlight(false);
		
		printf("Press B to continue without IOS Reload\n");
		
		waitforbuttonpress(&pressed, &pressedGC);
		
		if (pressed == WPAD_BUTTON_LEFT || pressedGC == PAD_BUTTON_LEFT)
		{	
			if (selection > 0)
			{
				selection--;
			} else
			{
				selection = ioscount - 1;
			}
		}
		if (pressed == WPAD_BUTTON_RIGHT || pressedGC == PAD_BUTTON_RIGHT)
		{
			if (selection < ioscount -1	)
			{
				selection++;
			} else
			{
				selection = 0;
			}
		}
		if (pressed == WPAD_BUTTON_A || pressedGC == PAD_BUTTON_A) break;
		if (pressed == WPAD_BUTTON_B || pressedGC == PAD_BUTTON_B)
		{
			return 0;
		}
		
	}
	return list[selection];
}



int miosinstallmenu()
{
	u32 pressed;
	u32 pressedGC;
	int selection = 0;
	bool options[2] = { true, false };
	int ret;

	char *optionsstring[2] = {	"Patch for homebrew&backups: ", "Set revision to 65535: " };

	while (true)
	{
		printf("\x1b[2J");
		printheadline();
		printf("\n");
		
		set_highlight(selection == 0);
		if (options[0] || options[1])
		{
			printf("Install patched MIOSv10\n");
		} else
		{
			printf("Install MIOSv10\n");
		}
		set_highlight(false);

		u32 i;
		for (i=0;i<2;i++)
		{
			printf(optionsstring[i]);
			set_highlight(selection == i+1);
			printf("%s\n", options[i] ? "yes" : "no");
			set_highlight(false);
		}

		printf("\n");
		
		waitforbuttonpress(&pressed, &pressedGC);
		
		if (pressed == WPAD_BUTTON_LEFT || pressedGC == PAD_BUTTON_LEFT)
		{	
			if (selection > 0)
			{
				options[selection-1] = !options[selection-1];
			}
		}

		if (pressed == WPAD_BUTTON_RIGHT || pressedGC == PAD_BUTTON_RIGHT)
		{	
			if (selection > 0)
			{
				options[selection-1] = !options[selection-1];
			}
		}
		
		if (pressed == WPAD_BUTTON_UP || pressedGC == PAD_BUTTON_UP)
		{
			if (selection > 0)
			{
				selection--;
			} else
			{
				selection = 2;
			}
		}

		if (pressed == WPAD_BUTTON_DOWN || pressedGC == PAD_BUTTON_DOWN)
		{
			if (selection < 2)
			{
				selection++;
			} else
			{
				selection = 0;
			}
		}

		if ((pressed == WPAD_BUTTON_A || pressedGC == PAD_BUTTON_A) && selection == 0)
		{
			u32 newrevision;
			if (options[1])
			{
				newrevision = 65535;
			} else
			{
				newrevision = 10;
			}
			ret = Install_patched_MIOS(0x101, 10, options[0], newrevision);
			if (ret < 0)
			{
				printf("MIOS installation failed.\n");
				return -1;
			}
			return 0;
		}
		
		if (pressed == WPAD_BUTTON_B || pressedGC == PAD_BUTTON_B)
		{
			printf("Installation cancelled\n");
			return 0;
		}		
	}	

}

int mainmenu()
{
	u32 pressed;
	u32 pressedGC;
	int selection = 0;
	int i;
	char *optionsstring[2] = { "MIOS menu", "Exit" };
	
	while (true)
	{
		printf("\x1b[2J");

		printheadline();
		printf("\n");
		
		for (i=0;i < 2;i++)
		{
			set_highlight(selection == i);
			printf("%s\n", optionsstring[i]);
			set_highlight(false);
		}
		printf("\n");
		
		waitforbuttonpress(&pressed, &pressedGC);
		
		if (pressed == WPAD_BUTTON_UP || pressedGC == PAD_BUTTON_UP)
		{
			if (selection > 0)
			{
				selection--;
			} else
			{
				selection = 1;
			}
		}

		if (pressed == WPAD_BUTTON_DOWN || pressedGC == PAD_BUTTON_DOWN)
		{
			if (selection < 1)
			{
				selection++;
			} else
			{
				selection = 0;
			}
		}

		if (pressed == WPAD_BUTTON_A || pressedGC == PAD_BUTTON_A)
		{
			if (selection == 0)
			{
				return miosinstallmenu();
			}

			if (selection == 1)
			{
				return 0;
			}
		}
		
		if (pressed == WPAD_BUTTON_B || pressedGC == PAD_BUTTON_B)
		{
			return 0;
		}		
	}	
}

int main(int argc, char* argv[])
{
	int ret;
	Init_Console();
	printf("\x1b[%u;%um", 37, false);

	PAD_Init();
	WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);					

	printheadline();
	
	ret = ios_selectionmenu(36);
	if (ret != 0)
	{
		WPAD_Shutdown();
		IOS_ReloadIOS(ret);
		PAD_Init();
		WPAD_Init();
		WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);					
	}

	printf("\n");
	
	printf("This application can brick your Wii, use it at your own risk!\n\n");
	printf("See readme for instructions and details.\n\n");
	
	time_t t = time(NULL) + 7;
	while (time(NULL) < t)
	{
		WPAD_ScanPads();
		PAD_ScanPads();
		if(WPAD_ButtonsDown(0) || PAD_ButtonsDown(0)) 
		{
			printf("Don't be impatient, press any button to exit...\n");
			waitforbuttonpress(NULL, NULL);
			Reboot();
		}
	}
	
	printf("Press 1 or Start to start the application...\n");

	u32 pressed;
	u32 pressedGC;	
	waitforbuttonpress(&pressed, &pressedGC);
	if (pressed != WPAD_BUTTON_1 && pressedGC != PAD_BUTTON_START)
	{
		printf("Other button pressed, press any button to exit...\n");
		waitforbuttonpress(NULL, NULL);
		Reboot();
	}

	mainmenu();
	
	Close_SD();
	Close_USB();
	
	printf("Press any button to exit...\n");
	waitforbuttonpress(NULL, NULL);
	
	Reboot();

	return 0;
}
