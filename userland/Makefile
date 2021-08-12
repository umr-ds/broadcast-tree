TARGET = btp
CC = gcc
INCLUDES = -Iinclude/
CFLAGS = -liw -lexplain -DLOG_USE_COLOR -g3 $(INCLUDES)

SOURCEDIR = src
BUILDDIR = build
INCLUDEDIR = include

SOURCES = $(wildcard $(SOURCEDIR)/*.c)
OBJECTS = $(patsubst $(SOURCEDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
HEADERS = $(wildcard $(INCLUDEDIR)/*.h)

.PHONY: default all clean

default: $(TARGET)
all: default

$(OBJECTS): $(BUILDDIR)/%.o: $(SOURCEDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -Wall -o $@

clean:
	-rm -rf $(BUILDDIR)
	-rm -f $(TARGET)