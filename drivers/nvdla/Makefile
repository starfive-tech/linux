
ccflags-y += -I$(srctree)/$(src)
ccflags-y += -I$(srctree)/$(src)/include

nvdla-y := scheduler.o \
	   engine.o \
	   bdma.o \
	   conv.o \
	   sdp.o \
	   cdp.o \
	   pdp.o \
	   rubik.o \
	   cache.o \
	   common.o \
	   engine_data.o \
	   engine_isr.o \
	   engine_debug.o \
	   nvdla_core_callbacks.o \
	   nvdla_gem.o

obj-$(CONFIG_NVDLA) += nvdla.o
