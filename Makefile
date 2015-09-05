# This line defines the name of the executables. Note that we assume
# the presence of an associated .cc file as well.
EXEC = bridge superviserOpt

# This line specifies all the other source files that need to be
# compiled and linked together with the executable
SOURCES = 

# This line specifies any flags that you want to feed the
# compiler, usually to find rhexlib and module include files.
AUXFLAGS = \
	-L$(LIBS_TOPDIR)/lib \
	-I$(SENSORHEX_TOPDIR)/include \
	-L$(SENSORHEX_TOPDIR)/lib \
	-I$(DY_DIRECTORY)/include \
	-L$(DY_DIRECTORY)/lib

# This line specifies any libraries that you want to link
# your executable against, usually containing modules and modes.
AUXLIBS += \
	$(SENSORHEX_TOPDIR)/lib/librhexmodules.a \
	$(LIBS_TOPDIR)/lib/libbase.a \
	$(LIBS_TOPDIR)/lib/libutils.a

ifeq ($(SENSORHEX_HARDWARE), _DUMMY_)
AUXLIBS += \
	$(RHEX_TOPDIR)/lib/librhexdummyhw.a \
	$(RHEX_TOPDIR)/lib/librhexcommonhw.a
endif

ifeq ($(SENSORHEX_HARDWARE), _URB_)

ifndef URB_DIR
$(error URB_DIR environment variable is not defined!)
endif
AUXFLAGS += \
	-I$(URB_DIR)/cpu_util/include -L$(URB_DIR)/cpu_util/urbcpu -static

AUXLIBS += \
	$(SENSORHEX_TOPDIR)/lib/librhexurbhw.a \
	$(SENSORHEX_TOPDIR)/lib/librhexcommonhw.a \
	$(URB_DIR)/cpu_util/urbcpu/liburbcpu.a
endif

ifeq ($(SENSORHEX_HARDWARE), _ARACHI_)
LDLIBS += -lXi -lXext -lX11 -lXmu -lGL -lGLU -lglut -lm -lqhull -ldynamism \
		-lboost_system -lboost_date_time -lboost_thread -lboost_program_options
AUXFLAGS += \
	-I$(ARACHI_DIR)/include -L$(ARACHI_DIR)/lib \
	-I$(SIMLIB_DIR)/include -L$(SIMLIB_DIR)/lib \
	-L/usr/X11R6/lib 

AUXLIBS += \
	$(SENSORHEX_TOPDIR)/lib/libarachihw.a \
	$(SENSORHEX_TOPDIR)/lib/librhexcommonhw.a \
	$(SIMLIB_DIR)/lib/libsimrti.a \
	$(SIMLIB_DIR)/lib/libsimpcm.a \
	$(SIMLIB_DIR)/lib/libsimcdl.a \
	$(SIMLIB_DIR)/lib/libsimpcm.a \
	$(SIMLIB_DIR)/lib/libsimrti.a \
	$(SIMLIB_DIR)/lib/libsimgraphics.a \
	$(ARACHI_DIR)/lib/libarachi.a

endif

LDLIBS += -lstdc++

###########################################################
# The rest of this file should not be modified            #
###########################################################

ifndef RHEXLIB_DIR
$(error RHEXLIB_DIR environment variable is not defined!)
endif

include $(RHEXLIB_DIR)/tools/bintargets.mk
