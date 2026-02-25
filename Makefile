CXX = clang

CXXFLAGS = -std=c23 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g -MMD

LDFLAGS = -fsanitize=address,undefined -g

TARGET = lispc.out
BUILD_DIR = build

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
	@echo "\n$(BOLD)$(GREEN)Compilation finished ✓$(RESET)"
	@echo "\n$(BOLD)$(CYAN)Linking ↺$(RESET)"
	@if ! $(CXX) $(LDFLAGS) -o $@ $^ > /dev/null ; then \
		echo "\n$(BOLD)$(RED)Linking failed ✘$(RESET)\n"; \
		exit 1;\
	fi

	@echo "$(BOLD)$(GREEN)Linking finished ✓$(RESET)"
	@echo "\n$(BOLD)$(CYAN)Executable dumped:$(RESET) $(BOLD)$(YELLOW)$(TARGET) ✓$(RESET)\n\n"

$(BUILD_DIR)/%.o: ./%.c
	@echo "\n$(BOLD)$(CYAN)Compiling $< ⧗$(RESET)"

	@mkdir -p $(dir $@)

	@if ! $(CXX) $(CXXFLAGS) -c $< -o $@ > /dev/null; then \
		echo "\n$(BOLD)$(RED)Compilation exited abnormally ✘$(RESET)\n\n"; \
		exit 1;\
	fi


clean:
	@echo "Cleaning build directory 🗑️"
	rm -rf $(BUILD_DIR)

clear:
	@echo "Clearing project directory 🗑️"
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)



