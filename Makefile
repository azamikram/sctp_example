MKDIR_P = mkdir -p

BUILD_DIR=build
SRCS=server.c client.c
INC=debug.h common.h
LIBS=-lsctp

OBJS=$(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))
CC=gcc -g

CFLAGS += -DERROR
CFLAGS += -DINFO
# CFLAGS += -DDEBUG

CFLAGS += -O3
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -fgnu89-inline

ifeq ($V,) # no echo
	export MSG=@echo
	export HIDE=@
else
	export MSG=@\#
	export HIDE=
endif

all: $(BUILD_DIR)

$(BUILD_DIR):
	$(MSG) "   MKDIR $@"
	$(HIDE) $(MKDIR_P) $@

$(OBJS): $(BUILD_DIR)/%.o: %.c $(INC)
	$(MSG) "   CC $<"
	$(HIDE) $(CC) -c $< $(CFLAGS) -o $@

%: $(BUILD_DIR) $(BUILD_DIR)/%.o
	$(MSG) "   LD $(BUILD_DIR)/$@.o"
	$(HIDE) $(CC) $(BUILD_DIR)/$@.o $(LIBS) -o $(BUILD_DIR)/$@

clean:
	$(MSG) "   CLEAN $(BUILD_DIR)"
	$(HIDE) rm -rf   $(BUILD_DIR)
