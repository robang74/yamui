PKG_NAMES += libdrm
PKG_NAMES += libpng

PKG_CONFIG := pkg-config
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKG_NAMES))
PKG_LDLIBS := $(shell $(PKG_CONFIG) --libs   $(PKG_NAMES))

CPPFLAGS += -D_GNU_SOURCE
CPPFLAGS += -DOVERSCAN_PERCENT=0

CFLAGS += -std=c99
CFLAGS += -O3
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += $(PKG_CFLAGS)
CFLAGS += -Wno-missing-field-initializers

LDLIBS += -Wl,--as-needed
LDLIBS += $(PKG_LDLIBS)

TARGETS_BIN += yamui
TARGETS_BIN += yamui-screensaverd
TARGETS_BIN += yamui-powerkey
TARGETS_BIN += mstime

DESTDIR ?= test-install-root # rpm-build overrides this

all:: $(TARGETS_BIN)
	strip $(TARGETS_BIN) ||:
	du -b $(TARGETS_BIN) ||:

install:: all
	install -m 755 -t $(DESTDIR)/usr/bin -D $(TARGETS_BIN)

distclean:: clean

clean:: mostlyclean
	$(RM) $(TARGETS_BIN)
	$(RM) *.o */*.o

mostlyclean::
	$(RM) *.bak *~ */*.bak */*~

MINUI_SRC += minui/graphics.c
MINUI_SRC += minui/events.c
MINUI_SRC += minui/resources.c
MINUI_SRC += minui/graphics_drm.c
MINUI_SRC += minui/graphics_fbdev.c

MSTIME_SRC += mstime.c
MSTIME_SRC += get_time_ms.c
MSTIME_OBJ := $(patsubst %.c, %.o, $(MSTIME_SRC))

mstime: $(MSTIME_OBJ)

YAMUI_SRC += yamui.c
YAMUI_SRC += os-update.c
YAMUI_SRC += get_time_ms.c
YAMUI_SRC += $(MINUI_SRC)
YAMUI_OBJ := $(patsubst %.c, %.o, $(YAMUI_SRC))

yamui: $(YAMUI_OBJ)

SCREENSAVERD_SRC += yamui-screensaverd.c
SCREENSAVERD_SRC += get_time_ms.c
SCREENSAVERD_SRC += yamui-tools.c
#SCREENSAVERD_SRC += $(MINUI_SRC)
SCREENSAVERD_OBJ := $(patsubst %.c, %.o, $(SCREENSAVERD_SRC))

yamui-screensaverd: $(SCREENSAVERD_OBJ)

POWERKEY_SRC += yamui-powerkey.c
POWERKEY_SRC += yamui-tools.c
POWERKEY_OBJ := $(patsubst %.c, %.o, $(POWERKEY_SRC))

yamui-powerkey: $(POWERKEY_OBJ)
