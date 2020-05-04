MKDIR_P = mkdir -p

BUILD_DIR=build
SRCS=server.c client.c
COMM=common.c
INC=debug.h common.h
BIN=server client
LIBS=-lsctp

_COMM_O=$(addprefix $(BUILD_DIR)/, $(COMM:.c=.o))
OBJS=$(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))
OBJS+=$(_COMM_O)

CC=gcc -g

# Three different level of trace functions
CFLAGS += -DERROR
CFLAGS += -DINFO
# CFLAGS += -DDEBUG

# Use the following flag to do rate calculation
CFLAGS += -DRATE

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

all: $(BUILD_DIR) $(BIN)

$(BUILD_DIR):
	$(MSG) "   MKDIR $@"
	$(HIDE) $(MKDIR_P) $@

$(OBJS): $(BUILD_DIR)/%.o: %.c $(INC)
	$(MSG) "   CC $<"
	$(HIDE) $(CC) -c $< $(CFLAGS) -o $@

%: $(BUILD_DIR) $(BUILD_DIR)/%.o $(_COMM_O)
	$(MSG) "   LD $(BUILD_DIR)/$@.o"
	$(HIDE) $(CC) $(BUILD_DIR)/$@.o $(COMM) $(LIBS) -o $(BUILD_DIR)/$@

clean:
	$(MSG) "   CLEAN $(BUILD_DIR)"
	$(HIDE) rm -rf   $(BUILD_DIR)
