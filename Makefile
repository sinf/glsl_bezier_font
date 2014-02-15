# todo: rewrite this ugly piece of shit

SHELL:=/bin/sh
NCPU ?=$(shell cat /proc/cpuinfo |grep -c processor)
CFLAGS ?=-g
# LDFLAGS ?= ...
# LDLIBS ?= ...

# Output binary size
OUTPUT:=prog

# Feel free to override any variables defined above (from command line ofc)
###########################

WARNINGS=-Wall -Wextra -Werror
PKG_CONFIG_ITEMS=glew gl glu
INCLUDE_FLAGS:=-Igpufont/include
THE_CC_FLAGS:=$(CFLAGS) $(shell pkg-config --cflags $(PKG_CONFIG_ITEMS)) $(shell sdl-config --cflags) $(WARNINGS) -ansi -pedantic -fopenmp $(INCLUDE_FLAGS)
THE_LINK_FLAGS:=$(LDFLAGS) -fopenmp
THE_LINK_LIBS:=$(LDLIBS) -Lgpufont -lgpufont $(shell pkg-config --libs $(PKG_CONFIG_ITEMS)) $(shell sdl-config --libs) -lm

# This folder is scanned recursively for .c and .h files. No nested directories
CODE_DIR:=code
# Only a single folder name allowed. No nested directories
BUILD_DIR:=build

# Basenames of all object files
OBJECTS_BASE:=$(shell find $(CODE_DIR) -type f -name '*.c' -printf '%f ' | sed 's_\.c _\.o _g')

#COMPILE_CMD:='	if [ ! -f $$@ ]; then $$(CC) $$(CFLAGS) $$(^:%.h=) -c -o $$@; fi'
COMPILE_CMD:='	$$(CC) $$(CFLAGS) -c $$< -o $$@'
LINK_CMD:='	$$(CC) $$(LDFLAGS) $$^ $$(LDLIBS) -o $$@'
DEPS_FILE:=$(BUILD_DIR)/Makefile.deps

###########################
###########################

.PHONY: all target again
target: $(OUTPUT)
	@echo "CFLAGS='$(CFLAGS)' LDFLAGS='$(LDFLAGS)'" > compiler_flags_used.txt
clean:
	rm -rf $(BUILD_DIR) $(OUTPUT)
	cd gpufont
	scons -c
again: clean target

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	ln -s ../$(CODE_DIR) $(BUILD_DIR)/$(CODE_DIR)

$(DEPS_FILE): $(BUILD_DIR)
	@echo '*** Generating Makefile.deps ***'
	date "+# %x %T" > $@
	echo '$$(OUTPUT):' "$(OBJECTS_BASE)" >> $@
	echo $(LINK_CMD) >> $@
	echo '%.h:' >> $@
	echo '%.o: %.c' >> $@
	echo $(COMPILE_CMD) >> $@
	find $(CODE_DIR) -name '*.c' | xargs -P $(NCPU) -n 1 gcc $(INCLUDE_FLAGS) -MM >> $@

GPUFONT_LIB:=gpufont/libgpufont.a
$(GPUFONT_LIB):
	cd gpufont
	scons

$(OUTPUT): $(DEPS_FILE) $(BUILD_DIR) $(GPUFONT_LIB)
	make -C $(BUILD_DIR) -f ../$(DEPS_FILE) SHELL="$(SHELL)" OUTPUT="../$(OUTPUT)" CFLAGS="$(THE_CC_FLAGS) " LDFLAGS="$(THE_LINK_FLAGS)" LDLIBS="$(THE_LINK_LIBS)" -j$(NCPU)

