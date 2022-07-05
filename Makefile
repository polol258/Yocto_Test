SRCS_CPP := \
   src/lte.cpp\
   src/atmodem.cpp

TARGET         = lte
TARGET_TYPE    = executable
OBJS           = $(SRCS:.c=.o) $(SRCS_CPP:.cpp=.o)
COMMON_FLAGS   = -Isrc/ -Iinclude/
EXTRA_CFLAGS   =
EXTRA_CXXFLAGS =
EXTRA_LDFLAGS  = -lpthread

# ============================================================
ifeq ($(TARGET_TYPE),shared_library)
ND_CFLAGS += -fPIC
LDFLAGS   := -shared -Wl,--no-undefined
else ifeq ($(TARGET_TYPE),executable)
ND_CFLAGS += -fPIE
LDFLAGS   := -pie
else ifeq ($(TARGET_TYPE),shared_library_no_dep)
ND_CFLAGS += -fPIC
LDFLAGS   := -shared
else ifeq ($(TARGET_TYPE),vendor_library)
ND_CFLAGS += -fPIC
LDFLAGS   := -shared
else ifeq ($(TARGET_TYPE),matrix_library)
ND_CFLAGS += -fPIC
LDFLAGS   := -shared -Wl,--no-undefined
endif

ND_CFLAGS += $(COMMON_FLAGS)
CFLAGS    = $(ND_CFLAGS) $(EXTRA_CFLAGS)
CXXFLAGS  = $(ND_CFLAGS) -std=c++11 $(EXTRA_CXXFLAGS)
LDFLAGS  += $(EXTRA_LDFLAGS)

CC    ?= $(CROSS_COMPILE)gcc
CXX   ?= $(CROSS_COMPILE)g++
STRIP ?= $(CROSS_COMPILE)strip
AR    ?= $(CROSS_COMPILE)ar

# supported source type to be compiled
ALL_OBJS := $(OBJS) \
            $(filter %.o, \
            $(LOCAL_SRCS:.cpp=.o)   \
            $(LOCAL_SRCS:.cxx=.o)   \
            $(LOCAL_SRCS:.cc=.o)    \
            $(LOCAL_SRCS:.c=.o)     \
            $(LOCAL_SRCS:.S=.o)     \
            $(LOCAL_SRCS:.s=.o) )

$(TARGET): LDFLAGS := $(LDFLAGS)
$(TARGET): CFLAGS := $(CFLAGS)
$(TARGET): CXXFLAGS := $(CXXFLAGS)
$(TARGET): TARGET := $(TARGET)
$(TARGET): $(ALL_OBJS)
clean-$(TARGET): ALL_OBJS := $(ALL_OBJS)
clean-$(TARGET): TARGET := $(TARGET)
clean-$(TARGET): ALL_RC := $(addprefix $(SYSTEM_SERVICE_CONFIG_PATH)/,$(notdir $(wildcard $(CP_RC))))
clean-$(TARGET): ALL_HEADERS := $(addprefix $(SYSTEM_INCLUDE_OUTPUT_PATH)/,$(notdir $(wildcard $(CP_HEADERS))))
clean-$(TARGET): VERSION := $(addprefix $(SYSTEM_API_VERSION_OUTPUT_PATH)/, \
                              $(word 1,$(subst :, ,$(API_VERSION))))

# clean up unused vars
OBJS:=
LOCAL_SRCS:=
ALL_OBJS:=
COMMON_FLAGS:=
EXTRA_CFLAGS:=
EXTRA_CXXFLAGS:=
EXTRA_LDFLAGS:=

.PHONY: clean-$(TARGET) clean

%.o : %.cxx
	@echo "======== CXX ======"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(TARGET):
	@echo "================ LINK $(TARGET) ==================="
	$(CXX) $(filter %.o,$(^)) -o $@ $(LDFLAGS)

clean-$(TARGET):
	@echo "---------------- CLEAN $(TARGET) -----------------"
	rm -r $(ALL_OBJS) $(TARGET) $(ALL_HEADERS) $(ALL_RC) $(VERSION)

clean: clean-$(TARGET)