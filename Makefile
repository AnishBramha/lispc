CC = cc

CFLAGS = -std=c23 -I. -Iinclude -Istd -Wall -Wextra -Wpedantic -Werror -Wno-sign-compare -MMD -fno-common -O3 -flto -march=native -DNDEBUG -fsanitize=address,undefined
LDFLAGS = -lm -O3 -flto -fsanitize=address,undefined

TARGET = target/lispc.out
BUILD_DIR = build
TARGET_DIR = target

SRCS = $(shell find . -name "*.c")
OBJS = $(patsubst ./%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(patsubst ./%.c,$(BUILD_DIR)/%.d,$(SRCS))

define COLOR_CODES
RED   	:= \033[31m
GREEN 	:= \033[32m
YELLOW 	:= \033[33m
MAGENTA := \033[35m
CYAN  	:= \033[36m
RESET   := \033[0m
BOLD  	:= \033[1m
endef

$(eval $(COLOR_CODES))

.PHONY: clean all clear

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	@printf "\n$(BOLD)$(GREEN)Compilation finished with code %d ✓$(RESET)\n" $$status;
	@echo "\n$(BOLD)$(CYAN)Linking ↺$(RESET)"
	@if ! $(CC) $(LDFLAGS) -o $@ $^ > /dev/null ; then \
		echo "\n$(BOLD)$(RED)Linking failed ✘$(RESET)\n"; \
		exit 1;\
	fi

	@echo "$(BOLD)$(GREEN)Linking finished ✓$(RESET)"
	@echo "\n$(BOLD)$(CYAN)Executable dumped:$(RESET) $(BOLD)$(YELLOW)$(TARGET) ✓$(RESET)\n\n"

$(BUILD_DIR)/%.o: ./%.c
	@echo "\n$(BOLD)$(CYAN)Compiling $< ⧗$(RESET)"

	@mkdir -p $(dir $@)

	@if ! $(CC) $(CFLAGS) -c $< -o $@ > /dev/null; then \
		printf "\n$(BOLD)$(RED)Compilation exited abnormally with code %d ✘ $(RESET)\n\n" $$status;\
		exit 1;\
	fi


clean:
	@echo "Cleaning build directory 🗑️"
	rm -rf $(BUILD_DIR)

clear:
	@echo "Clearing project directory 🗑️"
	rm -rf $(BUILD_DIR) $(TARGET) $(TARGET_DIR)

-include $(DEPS)



