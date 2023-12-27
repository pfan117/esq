#
# Need to download and build and install monkeycall project
#
# https://sourceforge.net/projects/monkeycall/
# cd monkeycall
# make
# make install
#
# Need to download and build and install libmco project
#
# https://gitee.com/pfan117/libmco
# cd libmco
# make install
# 
PROJDIR := ${CURDIR}
target := esq
mt_module := modules/mt.so
sfs_module := modules/simple-file-service.so
server_stop_module := modules/server-stop.so
foo_server_module := modules/foo_server.so
modules := $(sfs_module)
modules += $(foo_server_module) $(server_stop_module)
modules += $(remote_control_module)
CORE_SRC += $(wildcard src/*.c)
CORE_SRC += $(wildcard session/*.c)
CORE_SRC += $(wildcard mco/*.c)
CORE_SRC += $(wildcard pony/*.c)
CORE_SRC += $(wildcard bee/*.c)
CORE_SRC += $(wildcard utils/*.c)
CORE_SRC += $(wildcard libs/*/*.c)
CORE_SRC += $(wildcard nghttp2_wrapper/*.c)
C_SRC := $(CORE_SRC)
SRC :=
SRC += $(C_SRC)
OBJS := $(SRC:%.c=out/%.c.o)
deps := $(OBJS:%.o=%.d)
GCNOS := $(SRC:%.c=%.gcno)
GCDAS := $(SRC:%.c=%.gcda)

CFLAGS := -Wall -Werror -fPIC

CCINCLUDES := -I$(PROJDIR)
ifdef memcheck
CCINCLUDES += -I${HOME}/local-gcc-memcheck/include
LDFLAGS := -L${HOME}/local-gcc-memcheck/lib
endif
CCINCLUDES += -I${HOME}/local/include
LDFLAGS += -L${HOME}/local/lib

LDFLAGS += -ldl -lbfd -lpthread -lmonkeycall -lssl -lcrypto -lnghttp2 -lmco -lkyotocabinet -lbutter -luring

ifdef core_interface_test
CFLAGS += -DCORE_INTERFACE_TEST=
endif

ifdef profile
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -fprofile-arcs -ftest-coverage -lgcov
endif

ifdef gprof
CFLAGS += -DUSE_GRPOF
LDFLAGS += -lprofiler
endif

ifdef memcheck
CFLAGS += -fsanitize=address -fno-omit-frame-pointer
LDFLAGS += -lasan -fsanitize=address
endif

ifdef debug
CFLAGS += -ggdb3 -g3 -O0 -DSTATIC= -DDEBUG= 
BISONOPT := -rall
else
CFLAGS += -O2 -DSTATIC=static
BISONOPT :=
endif

ifdef mt
CFLAGS += -DMT_MODE
endif

ifdef range_map_check
CFLAGS += -DRANGE_MAP_CHECK_MODE
endif

GCC ?= clang
RM := rm

out/%.c.o:%.c
	@echo $(GCC) $<
	@$(GCC) $(CFLAGS) $(CCINCLUDES) -o $@ -MMD -MF out/$<.d -c $<

all: $(target) $(modules)

$(target): $(OBJS)
	@echo $(GCC) $(target)
	@$(GCC) -Wl,-E $^ -o $@ $(LDFLAGS)

mt: $(target) $(mt_module)

kill: force
	 @kill -9 `ps | grep esq | awk '{print $1}'`

# core events handling test module
MT_MOD_SRC := $(wildcard modules/mt/*.c)
MT_MOD_OBJ := $(MT_MOD_SRC:%.c=out/%.c.o)
MT_MOD_DEPS := $(MT_MOD_OBJ:%.o=%.d)
MT_MOD_GCDAS := $(MT_MOD_SRC:%.c=%.gcda)
MT_MOD_GCNOS := $(MT_MOD_SRC:%.c=%.gcno)
$(mt_module): $(MT_MOD_OBJ)
	@$(GCC) -shared -Wl,-E $^ -o $@ $(LDFLAGS)

# simple file server module
SIMPLE_FILE_SERVICE_MOD_SRC := $(wildcard modules/simple-file-service/*.c)
SIMPLE_FILE_SERVICE_MOD_OBJ := $(SIMPLE_FILE_SERVICE_MOD_SRC:%.c=out/%.c.o)
SIMPLE_FILE_SERVICE_MOD_DEPS := $(SIMPLE_FILE_SERVICE_MOD_OBJ:%.o=%.d)
SIMPLE_FILE_SERVICE_MOD_GCDAS := $(SIMPLE_FILE_SERVICE_MOD_SRC:%.c=%.gcda)
SIMPLE_FILE_SERVICE_MOD_GCNOS := $(SIMPLE_FILE_SERVICE_MOD_SRC:%.c=%.gcno)
$(sfs_module): $(SIMPLE_FILE_SERVICE_MOD_OBJ)
	@$(GCC) -shared -Wl,-E $^ -o $@ $(LDFLAGS)

# server stop module
SERVER_STOP_MOD_SRC := $(wildcard modules/server-stop/*.c)
SERVER_STOP_MOD_OBJ := $(SERVER_STOP_MOD_SRC:%.c=out/%.c.o)
SERVER_STOP_MOD_DEPS := $(SERVER_STOP_MOD_OBJ:%.o=%.d)
SERVER_STOP_MOD_GCDAS := $(SERVER_STOP_MOD_SRC:%.c=%.gcda)
SERVER_STOP_MOD_GCNOS := $(SERVER_STOP_MOD_SRC:%.c=%.gcno)
$(server_stop_module): $(SERVER_STOP_MOD_OBJ)
	@$(GCC) -shared -Wl,-E $^ -o $@ $(LDFLAGS)

# foo server module
FOO_SERVER_MOD_SRC := $(wildcard modules/foo_server/*.c)
FOO_SERVER_MOD_OBJ := $(FOO_SERVER_MOD_SRC:%.c=out/%.c.o)
FOO_SERVER_MOD_DEPS := $(FOO_SERVER_MOD_OBJ:%.o=%.d)
FOO_SERVER_MOD_GCDAS := $(FOO_SERVER_MOD_SRC:%.c=%.gcda)
FOO_SERVER_MOD_GCNOS := $(FOO_SERVER_MOD_SRC:%.c=%.gcno)
$(foo_server_module): $(FOO_SERVER_MOD_OBJ)
	@$(GCC) -shared -Wl,-E $^ -o $@ $(LDFLAGS)

clean: force
	@$(RM) -f $(OBJS)
	@$(RM) -f $(deps) $(MT_MOD_DEPS) $(SIMPLE_FILE_SERVICE_MOD_DEPS)
	@$(RM) -f $(SERVER_STOP_MOD_DEPS) $(FOO_SERVER_MOD_DEPS)
	@$(RM) -f $(GCNOS) $(GCDAS) $(target) $(modules) $(mt_module)
	@$(RM) -f $(FOO_SERVER_MOD_OBJ) $(FOO_SERVER_MOD_GCDAS) $(FOO_SERVER_MOD_GCNOS)
	@$(RM) -f $(MT_MOD_OBJ) $(MT_MOD_GCDAS) $(MT_MOD_GCNOS)
	@$(RM) -f $(SIMPLE_FILE_SERVICE_MOD_OBJ) $(SIMPLE_FILE_SERVICE_MOD_GCDAS) $(SIMPLE_FILE_SERVICE_MOD_GCNOS)
	@$(RM) -f $(SERVER_STOP_MOD_OBJ) $(SERVER_STOP_MOD_GCDAS) $(SERVER_STOP_MOD_GCNOS)
	@$(RM) -rf demo/local-files/upload/*
	@$(RM) -rf .c* *.log perf.* gprof.* *.tgz
	@$(RM) -rf valgrind-report.xml default.profraw

force:

-include $(deps)
-include $(MT_MOD_DEPS)
-include $(SIMPLE_FILE_SERVICE_MOD_DEPS)
-include $(SERVER_STOP_MOD_DEPS)
-include $(FOO_SERVER_MOD_DEPS)

# eof
