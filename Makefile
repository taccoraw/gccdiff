PREFIX := $(HOME)/INSTALL/gcc-4.8.4/bin/
GXX := $(PREFIX)g++
GCC := $(PREFIX)gcc
GCC_PLUGIN_DIR := $(shell $(GCC) -print-file-name=plugin)
NAME=demo
TARGET=$(NAME).so
all: $(TARGET)
$(TARGET): main.cxx
	$(GXX) -shared $^ -o $@ -I$(GCC_PLUGIN_DIR)/include -fPIC -O2 -I$(HOME)/SOURCE/build-gcc-4.8.4/gmp
test: hello.c $(TARGET)
	$(GCC) -fplugin=./$(TARGET) -fplugin-arg-$(NAME)-arg1=val1 $<
clean:
	rm -vf a.out $(TARGET)
.PHONY: all clean
