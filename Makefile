# see the readme for instructions: https://github.com/systemwii/make

# --- target ---
TARGET		:=	boot
TYPE		:=  dol
PLATFORM	:=	wii
BUILD		:=	_
CACHE		:=	_/cache

# --- sources ---
SRCS		 =	src
SRCEXTS		 =	.c
BINS		 =	$(wildcard lib/*/_) data
BINEXTS		 =	.bin .sys
LIBS		 =	-lrtip -lfat -lwiiuse -lbte -lmad -lm -lmodplay -logc
LIBDIRSBNDLE =	$(wildcard lib/*/_)
LIBDIRSLOOSE =	
INCLUDES	 =	
LIBOGC		 =	$(DEVKITPRO)/libogc

# --- flags --- # do not include MACHDEP here !!! (see readme linked at top)
CFLAGS		 =	-save-temps -g -O2 -Wall -Wno-unused-variable
CXXFLAGS	 =	$(CFLAGS)
ASFLAGS		 =	-D_LANGUAGE_ASSEMBLY
LDFLAGS		 =	-g -Wl,-Map,$(CACHE)/$(notdir $@).map
ARFLAGS		 =	rcs

# --- runs the templates ---
$(if $(findstring /,$(DEVKITPPC)),,$(error DEVKITPPC not set; run: export DEVKITPPC=<path to>devkitPPC))
RULESDIR	:=	lib/make/rules
include $(RULESDIR)/main.mk
