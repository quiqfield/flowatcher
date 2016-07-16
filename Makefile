ifeq ($(RTE_SDK),)
	$(error "Please define RTE_SDK environment variable")
endif

RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = flowatcher

# all source are stored in SRCS-y
SRCS-y := src/main.c
SRCS-y += src/init.c
SRCS-y += src/process_rx_flow.c
SRCS-y += src/process_monitor.c
SRCS-y += src/process_skelton.c
SRCS-y += src/flowtable.c
SRCS-y += src/lws_callback_http.c
SRCS-y += src/get_app_status.c

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-unused-but-set-variable

# libwebsockets
CFLAGS += -I./deps/libwebsockets/target/include
LDLIBS += -L./deps/libwebsockets/target/lib -lwebsockets

include $(RTE_SDK)/mk/rte.extapp.mk

.PHONY: run
run:
	./build/$(APP) -c f -n 4
