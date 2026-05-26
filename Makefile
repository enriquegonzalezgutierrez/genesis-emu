# ==============================================================================
# GenesisEmu - Master Makefile (Updated with Dockerized Clean)
# ==============================================================================
# This Makefile orchestrates all development, building, testing, and execution
# tasks. It compiles inside Docker and executes natively on WSL/WSLg.
# Note: Cleanup is performed inside Docker to prevent 'Permission Denied' errors.
# ==============================================================================

.DEFAULT_GOAL := help

# --- Docker Compose Commands ---
DOCKER_COMPOSE = docker compose

# --- Colors for Help Output ---
YELLOW = \033[33m
RESET  = \033[0m
GREEN  = \033[32m

# ------------------------------------------------------------------------------
# DEVELOPMENT & EXECUTION TARGETS
# ------------------------------------------------------------------------------

.PHONY: setup
setup: ## Build the compiler Docker image
	@echo "$(YELLOW)Building Docker environment...$(RESET)"
	$(DOCKER_COMPOSE) build

.PHONY: test
test: ## Run the TDD Unit Test suite inside Docker
	@echo "$(YELLOW)Running TDD Unit Tests...$(RESET)"
	$(DOCKER_COMPOSE) run --rm test

.PHONY: build
build: ## Compile the emulator in Release mode inside Docker
	@echo "$(YELLOW)Compiling production binary...$(RESET)"
	$(DOCKER_COMPOSE) run --rm build

.PHONY: run
run: build ## Compile and launch the emulator natively on the host (WSLg)
	@echo "$(YELLOW)Launching GenesisEmu on host graphics server...$(RESET)"
	@./build_release/bin/GenesisEmu

.PHONY: shell
shell: ## Open an interactive bash shell inside the compiler container
	@echo "$(YELLOW)Entering container shell...$(RESET)"
	$(DOCKER_COMPOSE) run --rm dev

.PHONY: clean
clean: ## Remove compiler cache and artifact directories (runs inside Docker to avoid permission issues)
	@echo "$(YELLOW)Cleaning up build artifacts inside Docker...$(RESET)"
	$(DOCKER_COMPOSE) run --rm compiler_base rm -rf build build_release
	@echo "$(GREEN)Cleanup complete.$(RESET)"

# ------------------------------------------------------------------------------
# DOCUMENTATION & HELP
# ------------------------------------------------------------------------------

.PHONY: help
help: ## Display this help message
	@echo "=============================================================================="
	@echo " GenesisEmu Build & Execution System"
	@echo "=============================================================================="
	@echo "Available commands:"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  $(GREEN)%-15s$(RESET) %s\n", $$1, $$2}'
	@echo "=============================================================================="