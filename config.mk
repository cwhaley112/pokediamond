GAME_VERSION ?= DIAMOND
GAME_LANGUAGE ?= ENGLISH
COMPARE := 0
SHIFTED ?= 0
ifneq ($(SHIFTED),0)
COMPARE := 0
endif

ifeq ($(GAME_VERSION),DIAMOND)
GAME_CODE := ADA
BUILD_NAME := diamond
TITLE_NAME := POKEMON D
ICON_FILE := graphics/icon.png
else
ifeq ($(GAME_VERSION),PEARL)
GAME_CODE := APA
BUILD_NAME := pearl
TITLE_NAME := POKEMON P
ICON_FILE := graphics/icon_pearl.png
else
$(error Invalid GAME_VERSION: $(GAME_VERSION))
endif
endif

ifeq ($(GAME_LANGUAGE),ENGLISH)
GAME_CODE := $(GAME_CODE)E
BUILD_NAME := $(BUILD_NAME).us
GAME_REVISION := 5
else
$(error Invalid GAME_LANGUAGE: $(GAME_LANGUAGE))
endif

ifeq ($(GAME_CODE),ADAE)
SECURE_CRC := 0x5931
else
ifeq ($(GAME_CODE),APAE)
SECURE_CRC := 0x014C
else
$(error Unsupported build: $(GAME_VERSION) $(GAME_LANGUAGE))
endif
endif

BUILD_TARGET := poke$(BUILD_NAME)

MAKE_VARS := SHIFTED=$(SHIFTED) COMPARE=$(COMPARE) GAME_LANGUAGE=$(GAME_LANGUAGE) GAME_VERSION=$(GAME_VERSION)
