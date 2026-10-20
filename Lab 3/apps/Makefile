ARM_CC ?= arm-linux-gnueabihf-gcc-11
HOST_CC ?= gcc-11

ARM_FLAGS := -static -g3 -O0 -fno-omit-frame-pointer \
             -march=armv7-a -mfpu=vfpv3-d16 \
             -mfloat-abi=hard -Wall -Wextra

HOST_FLAGS := -g3 -O0 -fno-omit-frame-pointer -Wall -Wextra

.PHONY: all clean

all: buggy_app buggy_app_fixed_arm \
     leaky_app leaky_app_fixed leaky_app_arm \
     perf_target perf_target_arm

buggy_app: buggy_app.c
	$(ARM_CC) $(ARM_FLAGS) -fno-inline -fwrapv -o $@ $<

buggy_app_fixed_arm: buggy_app_fixed.c
	$(ARM_CC) $(ARM_FLAGS) -o $@ $<

leaky_app: leaky_app.c
	$(HOST_CC) $(HOST_FLAGS) -o $@ $<

leaky_app_fixed: leaky_app_fixed.c
	$(HOST_CC) $(HOST_FLAGS) -o $@ $<

leaky_app_arm: leaky_app.c
	$(ARM_CC) $(ARM_FLAGS) -o $@ $<

perf_target: perf_target.c
	$(HOST_CC) $(HOST_FLAGS) -o $@ $< -lm

perf_target_arm: perf_target.c
	$(ARM_CC) $(ARM_FLAGS) -o $@ $< -lm

clean:
	rm -f buggy_app buggy_app_fixed_arm
	rm -f leaky_app leaky_app_fixed leaky_app_arm
	rm -f perf_target perf_target_arm
