## Usage
#
#   $ make install     # run install process(es)
#   $ make compile     # compile the device firmware
#   $ make upload      # compile then upload the device firmware
#   $ make monitor     # start the serial monitor
#

## Variables
DEVICE ?= /dev/ttyUSB0
BAUDRATE ?= 115200
PLATFORM=espressif32

## Phony targets - these are for when the target-side of a definition
# (such as "install" below) isn't a file but instead just a label. Declaring
# it as phony ensures that it always run, even if a file by the same name
# exists.
.PHONY: install\
compile\
upload\
monitor

install:
	pio pkg install --platform ${PLATFORM}

compile:
	pio run

upload:
	sudo chown ${USER}:${USER} ${DEVICE}
	pio run --upload-port ${DEVICE} --target upload

monitor:
	sudo chown ${USER}:${USER} ${DEVICE}
	pio device monitor --baud ${BAUDRATE} --port ${DEVICE}
