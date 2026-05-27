# ==============================================================================
# GenesisEmu - Master Developer Workflow Makefile
# ==============================================================================
# This file orchestrates development, building, testing, and execution tasks.
# It compiles within isolated Docker layers and runs presentation layers on host.
# ==============================================================================

.DEFAULT_GOAL := help

# --- Docker Compose Commands ---
DOCKER_COMPOSE = docker compose

# --- Output Decoration Colors ---
YELLOW = \033[33m
RESET  = \033[0m
GREEN  = \033[32m

# --- Configuration Parameters ---
# Allows passing custom ROM targets via shell parameters: make run ROM=roms/sonic.bin
ROM ?= roms/final_fight_md.bin

# ------------------------------------------------------------------------------
# DEVELOPMENT & COMPILATION TARGETS
# ------------------------------------------------------------------------------

.PHONY: setup
setup: ## Build the isolated compiler Docker container
	@echo "$(YELLOW)Building compiler environment...$(RESET)"
	$(DOCKER_COMPOSE) build

.PHONY: test
test: ## Compile and execute the GTest Unit Tests suite inside Docker
	@echo "$(YELLOW)Running Core TDD Unit Tests...$(RESET)"
	$(DOCKER_COMPOSE) run --rm test

.PHONY: build
build: ## Compile the emulator in Release mode inside Docker
	@echo "$(YELLOW)Compiling optimized Release binaries...$(RESET)"
	$(DOCKER_COMPOSE) run --rm build

.PHONY: run
run: build ## Compile and launch the emulator natively on the host (WSLg)
	@echo "$(YELLOW)Launching GenesisEmu on host graphics server...$(RESET)"
	@echo "$(GREEN)Target ROM: $(ROM)$(RESET)"
	@./build_release/bin/GenesisEmu $(ROM)

.PHONY: shell
shell: ## Enter the interactive terminal of the compiler container
	@echo "$(YELLOW)Entering compiler shell...$(RESET)"
	$(DOCKER_COMPOSE) run --rm dev

.PHONY: clean
clean: ## Remove compiler cache and artifact directories
	@echo "$(YELLOW)Cleaning up compiled cache files...$(RESET)"
	$(DOCKER_COMPOSE) run --rm compiler_base rm -rf build build_release
	@echo "$(GREEN)Cleanup complete.$(RESET)"

# ------------------------------------------------------------------------------
# TELEMETRY & DOCUMENTATION HELPERS
# ------------------------------------------------------------------------------

.PHONY: help
help: ## Display this diagnostic help card
	@echo "=============================================================================="
	@echo " GenesisEmu Build & Execution System"
	@echo "=============================================================================="
	@echo "Available commands:"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  $(GREEN)%-15s$(RESET) %s\n", $$1, $$2}'
	@echo "=============================================================================="
	@echo "Example: make run ROM=roms/240pSuite-1.32.bin"
	@echo "=============================================================================="