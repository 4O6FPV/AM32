MCU := G3507
PART := MSPM0G3507

MCU_LC := $(call lc,$(MCU))

TARGETS_$(MCU) := $(call get_targets,$(MCU))

HAL_FOLDER_$(MCU) := $(HAL_FOLDER)/$(MCU_LC)

MCU_$(MCU) := -mcpu=cortex-m0plus -mthumb
LDSCRIPT_$(MCU) := $(wildcard $(HAL_FOLDER_$(MCU))/*.ld)

# TI MSPM0 SDK subset vendored under Mcu/g3507/Drivers (see its README.md)
SRC_BASE_DIR_$(MCU) := \
	$(HAL_FOLDER_$(MCU))/Startup \
	$(HAL_FOLDER_$(MCU))/Drivers/ti/driverlib \
	$(HAL_FOLDER_$(MCU))/Drivers/ti/driverlib/m0p \
	$(HAL_FOLDER_$(MCU))/Drivers/ti/driverlib/m0p/sysctl

SRC_DIR_$(MCU) := $(SRC_BASE_DIR_$(MCU)) \
	$(HAL_FOLDER_$(MCU))/Src

CFLAGS_$(MCU) := \
	-I$(HAL_FOLDER_$(MCU))/Inc \
	-I$(HAL_FOLDER_$(MCU))/Drivers \
	-I$(HAL_FOLDER_$(MCU))/Drivers/CMSIS/Core/Include

CFLAGS_$(MCU) += \
	-D__$(PART)__

# TI DriverLib's dl_timer.c (capture helpers this port does not call) trips
# GCC's maybe-uninitialized analysis; keep it a warning rather than editing
# the vendored file
CFLAGS_$(MCU) += -Wno-error=maybe-uninitialized

SRC_$(MCU) := $(foreach dir,$(SRC_DIR_$(MCU)),$(wildcard $(dir)/*.[cs]))
