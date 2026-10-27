ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
DEPS := $(ROOT)/pi-deps
OUT := $(ROOT)/pi-build
DATA ?= $(HOME)/.local/share/rumy
VERSION := $(shell sed -n 's/^version *= *//p' $(ROOT)/platformio.ini | head -n 1)

CC := gcc
CXX := g++
ARCH := -mcpu=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard
DEFS := -DLV_CONF_INCLUDE_SIMPLE -DRUMY_PI=1 -DRUMY_LVGL_POOL_KB=96 -DRUMY_WEB_PORT=8080 -DRUMY_TOUCH_ROT=2 \
	-DRUMY_SCREEN_W=480 -DRUMY_SCREEN_H=320 \
	-DRUMY_VERSION=\"$(VERSION)\" -DRUMY_DATA_DIR=\"$(DATA)\" -DRUMY_SD_ROOT=\"$(DATA)/sd\" \
	-DARDUINOJSON_USE_DOUBLE=0 -DARDUINOJSON_USE_LONG_LONG=0 -DARDUINOJSON_POOL_CAPACITY=32
INC := -I$(ROOT)/pi -I$(ROOT)/pi/include -I$(ROOT)/include -I$(DEPS)/lvgl -I$(DEPS) -I$(ROOT)/src \
	-I$(ROOT)/lib/rumy_core/src -I$(DEPS)/ArduinoJson/src -I$(DEPS)/tjpgd
CFLAGS := -O2 $(ARCH) -std=gnu11 -w $(DEFS) $(INC) -MMD -MP
CXXFLAGS := -O2 $(ARCH) -std=gnu++17 -Wall -Wextra $(DEFS) $(INC) -MMD -MP
LDLIBS := -lpthread -lm -l:libcurl.so.4
STAMP := $(OUT)/app/.bayraklar
$(shell mkdir -p $(OUT)/app && echo '$(CXX) $(CXXFLAGS) $(CFLAGS)' | cmp -s - $(STAMP) || \
	echo '$(CXX) $(CXXFLAGS) $(CFLAGS)' > $(STAMP))

LVGL_SRC := $(shell find $(DEPS)/lvgl/src -name '*.c' -not -path '*/drivers/*' -not -path '*/libs/thorvg/*' \
	-not -path '*/others/vg_lite_tvg/*')
LVGL_OBJ := $(patsubst $(DEPS)/%.c,$(OUT)/deps/%.o,$(LVGL_SRC))

APP_CPP := $(wildcard $(ROOT)/lib/rumy_core/src/*.cpp) $(wildcard $(ROOT)/src/ui/*.cpp) \
	$(wildcard $(ROOT)/src/rumy/*.cpp) $(ROOT)/src/settings/settings.cpp $(ROOT)/src/settings/provinces.cpp \
	$(ROOT)/src/app/store.cpp $(ROOT)/src/app/mem.cpp $(ROOT)/src/weather/weather.cpp \
	$(ROOT)/src/quake/quake.cpp $(ROOT)/src/events/events.cpp $(ROOT)/src/net/cache.cpp \
	$(ROOT)/src/media/media.cpp $(ROOT)/src/media/camera.cpp $(ROOT)/src/media/bridge.cpp $(ROOT)/src/media/fwupdate.cpp $(ROOT)/src/net/relay.cpp $(ROOT)/src/wiz/wiz.cpp \
	$(ROOT)/src/printer/printer.cpp $(ROOT)/src/match/match.cpp $(ROOT)/src/match/board.cpp $(ROOT)/src/match/leagues.cpp $(ROOT)/src/match/fast.cpp $(ROOT)/src/agents/agents.cpp $(ROOT)/src/diag/diag.cpp $(ROOT)/src/webui/webui.cpp \
	$(wildcard $(ROOT)/pi/src/*.cpp)
APP_C := $(wildcard $(ROOT)/src/ui/fonts/*.c)
APP_OBJ := $(patsubst $(ROOT)/%.cpp,$(OUT)/app/%.o,$(APP_CPP)) $(patsubst $(ROOT)/%.c,$(OUT)/app/%.o,$(APP_C)) \
	$(OUT)/deps/tjpgd/lgfx_tjpgd.o

.PHONY: all lvgl app clean
all: $(OUT)/rumy

lvgl: $(OUT)/liblvgl.a

app: $(APP_OBJ)

$(OUT)/liblvgl.a: $(LVGL_OBJ)
	@rm -f $@.tmp
	ar rcs $@.tmp $^ && mv -f $@.tmp $@

$(OUT)/rumy: $(APP_OBJ) $(OUT)/liblvgl.a
	$(CXX) -o $@.tmp $(APP_OBJ) $(OUT)/liblvgl.a $(LDLIBS) && { [ ! -f $@ ] || ln -f $@ $@.onceki; } && \
	mv -f $@.tmp $@

$(OUT)/deps/%.o: $(DEPS)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MF $(@:.o=.d) -MT $@ -c $< -o $@.tmp && mv -f $@.tmp $@

$(OUT)/app/%.o: $(ROOT)/%.cpp $(STAMP)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MF $(@:.o=.d) -MT $@ -c $< -o $@.tmp && mv -f $@.tmp $@

$(OUT)/app/%.o: $(ROOT)/%.c $(STAMP)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MF $(@:.o=.d) -MT $@ -c $< -o $@.tmp && mv -f $@.tmp $@

clean:
	rm -rf $(OUT)

-include $(LVGL_OBJ:.o=.d) $(APP_OBJ:.o=.d)
