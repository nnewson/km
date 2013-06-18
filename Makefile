# Project Name (.ko)
PROJECT = km

# Directories
SOURCE_DIRECTORY = src

# Symbol template and output
SYMBOL_TEMPLATE = symbols_template.h
SYMBOL_OUTPUT = symbols.h

#-- The project files --

# Add all the source files
SOURCE_FILES = $(shell ls $(PWD)/$(SOURCE_DIRECTORY)/*.c)

# Create an object file of every c file
OBJECTS = $(patsubst $(PWD)/%.c,%.o,$(SOURCE_FILES))

# Find symbols in System.map
MODULES_SYMBOL = $(shell grep -w modules /boot/System.map-$(shell uname -r) | awk '{print $$1}')
SYSFS_MUTEX_SYMBOL = $(shell grep -w sysfs_mutex /boot/System.map-$(shell uname -r) | awk '{print $$1}')
SYSFS_UNLINK_SIBLING_SYMBOL = $(shell grep -w sysfs_unlink_sibling /boot/System.map-$(shell uname -r) | awk '{print $$1}')
SYSFS_LINK_SIBLING_SYMBOL = $(shell grep -w sysfs_link_sibling /boot/System.map-$(shell uname -r) | awk '{print $$1}')

#-- Build the project --

obj-m += $(PROJECT).o
$(PROJECT)-objs = $(OBJECTS)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

.PHONY : symbols
symbols:
	sed -e s/MODULES_SYMBOL/$(MODULES_SYMBOL)/g \
		-e s/SYSFS_MUTEX_SYMBOL/$(SYSFS_MUTEX_SYMBOL)/g \
		-e s/SYSFS_UNLINK_SIBLING_SYMBOL/$(SYSFS_UNLINK_SIBLING_SYMBOL)/g \
		-e s/SYSFS_LINK_SIBLING_SYMBOL/$(SYSFS_LINK_SIBLING_SYMBOL)/g \
		$(SOURCE_DIRECTORY)/$(SYMBOL_TEMPLATE) > $(SOURCE_DIRECTORY)/$(SYMBOL_OUTPUT)

.PHONY : clean
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
