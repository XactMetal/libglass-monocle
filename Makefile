JFX_JAR = /usr/lib/jvm/java-8-openjdk-arm64/jre/lib/ext/jfxrt.jar 
JDK_HOME = /usr/lib/jvm/java-8-openjdk-arm64
JAVAH = javah -cp $(JFX_JAR)
CC = gcc # C compiler
CFLAGS = -fno-strict-aliasing -fPIC -fno-omit-frame-pointer -W -Wall -Wno-unused -Wno-parentheses -Werror=implicit-function-declaration -I/usr/include/drm -I/usr/include/libdrm -I$(JDK_HOME)/include -I$(JDK_HOME)/include/linux -c -O2 -DNDEBUG -Wno-misleading-indentation -Werror -I./ # C flags
LDFLAGS = -fno-strict-aliasing -fPIC -fno-omit-frame-pointer -W -Wall -Wno-unused -Wno-parentheses -Werror=implicit-function-declaration -shared -L/usr/lib/arm-linux-gnueabihf -ldl -lm # linking flags
RM = rm -f  # rm command
TARGET_LIB = libglass_monocle.so # target lib

SRCS = EGL.c util/C.c x11/X11.c mx6/MX6AcceleratedScreen.c linux/Udev.c linux/LinuxSystem.c # source files
OBJS = $(SRCS:.c=.o)
.PHONY: all 
all: ${TARGET_LIB}


$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

include $(SRCS:.c=.d)

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.c=.d)

