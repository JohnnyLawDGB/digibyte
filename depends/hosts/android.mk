ifeq ($(HOST),armv7a-linux-android)
<<<<<<< HEAD
android_AR=$(ANDROID_TOOLCHAIN_BIN)/arm-linux-androideabi-ar
android_CXX=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)eabi$(ANDROID_API_LEVEL)-clang++
android_CC=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)eabi$(ANDROID_API_LEVEL)-clang
android_RANLIB=$(ANDROID_TOOLCHAIN_BIN)/arm-linux-androideabi-ranlib
else
android_AR=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)-ar
android_CXX=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)$(ANDROID_API_LEVEL)-clang++
android_CC=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)$(ANDROID_API_LEVEL)-clang
android_RANLIB=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)-ranlib
endif
=======
android_CXX=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)eabi$(ANDROID_API_LEVEL)-clang++
android_CC=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)eabi$(ANDROID_API_LEVEL)-clang
else
android_CXX=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)$(ANDROID_API_LEVEL)-clang++
android_CC=$(ANDROID_TOOLCHAIN_BIN)/$(HOST)$(ANDROID_API_LEVEL)-clang
endif

android_CFLAGS=-std=$(C_STANDARD)
android_CXXFLAGS=-std=$(CXX_STANDARD)

ifneq ($(LTO),)
android_CFLAGS += -flto
android_LDFLAGS += -flto
endif

android_AR=$(ANDROID_TOOLCHAIN_BIN)/llvm-ar
android_RANLIB=$(ANDROID_TOOLCHAIN_BIN)/llvm-ranlib

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
android_cmake_system=Android
