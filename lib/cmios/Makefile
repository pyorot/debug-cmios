# see the readme for instructions: https://github.com/systemwii/make

# --- target ---
TARGET		:=	dolloader
TYPE		:=  bin
PLATFORM	:=	gamecube
BUILD		:=	_
CACHE		:=	_/cache

# --- sources ---
SRCS		 =	src ../monke/sha1 ../monke/console
SRCEXTS		 =	.c
BINS		 =	data
BINEXTS		 =	.dol
LIBS		 =	-logc
LIBDIRSBNDLE =	
LIBDIRSLOOSE =	
INCLUDES	 =	

# --- flags ---
CFLAGS		 =	-save-temps -g -Os -Wall -Wno-unused-variable
CXXFLAGS	 =	$(CFLAGS)
ASFLAGS		 =	-D_LANGUAGE_ASSEMBLY
LDFLAGS		 =	-g -Wl,--section-start,.init=0x81000000,-Map,$(CACHE)/$(notdir $@).map
ARFLAGS		 =	rcs

# --- runs the templates ---
$(if $(findstring /,$(DEVKITPPC)),,$(error DEVKITPPC not set; run: export DEVKITPPC=<path to>devkitPPC))
RULESDIR	:=	../make/rules
include $(RULESDIR)/main.mk
