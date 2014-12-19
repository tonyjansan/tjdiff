# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

#LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wall -Os -O2 -fvisibility=hidden -nostdlib
# global defination
LOCAL_CFLAGS += -DCROSS_COMPILE -DJNI_ONLOAD

ifeq ($(TARGET_ARCH),arm)
# float improvement
LOCAL_CFLAGS += -mfpu=vfp
#LOCAL_CPPFLAGS += -mfloat-abi=softfp

# cpu instruction (arm/thumb)
# LOCAL_ARM_MODE := arm
endif

BASE_SRC_FILES := ../libbzip2/blocksort.c ../libbzip2/bzlib.c ../libbzip2/compress.c \
				  ../libbzip2/crctable.c ../libbzip2/decompress.c ../libbzip2/huffman.c \
				  ../libbzip2/randtable.c ../btutils.c

LOCAL_MODULE := btpatch
LOCAL_SRC_FILES := ../btpatch.c $(BASE_SRC_FILES)

include $(BUILD_EXECUTABLE)
