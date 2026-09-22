CXX ?= g++
TARGET ?= rtf2pdf
BUILD_DIR ?= build
CONFIG ?= release

THIRDPARTY_DIR ?= ../thirdparty
PDFWRITER_DIR ?= $(THIRDPARTY_DIR)/PDF-Writer
PUGIXML_DIR ?= $(THIRDPARTY_DIR)/pugixml-1.15
PDFWRITER_LIB_DIR ?= $(PDFWRITER_DIR)/lib
PUGIXML_LIB_DIR ?= $(THIRDPARTY_DIR)/pugixml-1.15.lib/lib

SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

CPPFLAGS += -Isrc \
            -I$(PDFWRITER_DIR)/PDFWriter \
            -I$(PDFWRITER_DIR)/FreeType/include \
            -I$(PUGIXML_DIR)/src
CXXFLAGS += -std=c++20 -Wall -Wextra -MMD -MP
LDFLAGS += -L$(PDFWRITER_LIB_DIR) -L$(PUGIXML_LIB_DIR)
PDFWRITER_LIBS ?= -lPDFWriter -lLibPng -lLibTiff -lFreeType -lLibAesgm -lLibJpeg -lZlib
LDLIBS := $(PDFWRITER_LIBS) -lpugixml -pthread

ifeq ($(CONFIG),debug)
  CXXFLAGS += -O0 -g
else
  CPPFLAGS += -DNDEBUG
  CXXFLAGS += -O2
endif

.PHONY: all clean debug release check-deps help

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

debug:
	$(MAKE) CONFIG=debug

release:
	$(MAKE) CONFIG=release

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

check-deps:
	@test -f "$(PDFWRITER_DIR)/PDFWriter/PDFWriter.h" || { echo "Missing PDF-Writer headers under $(PDFWRITER_DIR)"; exit 1; }
	@test -f "$(PUGIXML_DIR)/src/pugixml.hpp" || { echo "Missing pugixml headers under $(PUGIXML_DIR)"; exit 1; }
	@echo "Dependency headers found."

help:
	@printf '%s\n' 'Targets: all (default), debug, release, clean'
	@printf '%s\n' 'Targets: check-deps'
	@printf '%s\n' 'Override dependency roots with THIRDPARTY_DIR, PDFWRITER_DIR, and PUGIXML_DIR.'

-include $(OBJECTS:.o=.d)