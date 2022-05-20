LOCAL_DIR := $(GET_LOCAL_DIR)

MODULES += \
	lib/fs \
	lib/bio \
	lib/partition

OBJS += \
	$(LOCAL_DIR)/boot.o
