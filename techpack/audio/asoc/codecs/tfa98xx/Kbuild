# We can build either as part of a standalone Kernel build or as
# an external module.  Determine which mechanism is being used
ifeq ($(MODNAME),)
	KERNEL_BUILD := 1
else
	KERNEL_BUILD := 0
endif

ifeq ($(KERNEL_BUILD), 1)
	AUDIO_ROOT := $(srctree)/techpack/audio
else
	ifeq ($(CONFIG_ARCH_LAHAINA), y)
		include $(AUDIO_ROOT)/config/lahainaauto.conf
		INCS    +=  -include $(AUDIO_ROOT)/config/lahainaautoconf.h
	endif
endif

############ UAPI ############
UAPI_INC :=	-I$(AUDIO_ROOT)/include/uapi/audio

############ COMMON ############
COMMON_INC :=	-I$(AUDIO_ROOT)/include

############ TFA98XX ############
PRIV_INC := -I$(AUDIO_ROOT)/asoc/codecs/tfa98xx/inc

# for TFA98XX Codec
ifdef CONFIG_SND_SOC_TFA98XX
	TFA98XX_OBJS += src/tfa98xx.o
	TFA98XX_OBJS += src/tfa_container.o
	TFA98XX_OBJS += src/tfa_dsp.o
	TFA98XX_OBJS += src/tfa_init.o
endif

LINUX_INC += -Iinclude/linux

INCS +=	$(COMMON_INC) $(UAPI_INC) $(PRIV_INC)

EXTRA_CFLAGS += $(INCS)

CDEFINES +=	-DANI_LITTLE_BYTE_ENDIAN \
		-DANI_LITTLE_BIT_ENDIAN \
		-DDOT11F_LITTLE_ENDIAN_HOST \
		-DANI_COMPILER_TYPE_GCC \
		-DANI_OS_TYPE_ANDROID=6 \
		-DPTT_SOCK_SVC_ENABLE \
		-Wall\
		-Werror\
		-D__linux__

KBUILD_CPPFLAGS += $(CDEFINES)

# Currently, for versions of gcc which support it, the kernel Makefile
# is disabling the maybe-uninitialized warning.  Re-enable it for the
# AUDIO driver.  Note that we must use EXTRA_CFLAGS here so that it
# will override the kernel settings.
ifeq ($(call cc-option-yn, -Wmaybe-uninitialized),y)
EXTRA_CFLAGS += -Wmaybe-uninitialized
endif
ifeq ($(call cc-option-yn, -Wheader-guard),y)
EXTRA_CFLAGS += -Wheader-guard
endif

# Module information used by KBuild framework
obj-$(CONFIG_SND_SOC_TFA98XX) += tfa98xx_dlkm.o
tfa98xx_dlkm-y := $(TFA98XX_OBJS)

# inject some build related information
DEFINES += -DBUILD_TIMESTAMP=\"$(shell date -u +'%Y-%m-%dT%H:%M:%SZ')\"
