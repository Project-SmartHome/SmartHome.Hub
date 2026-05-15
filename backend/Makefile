.PHONY: build run clean test dev setup install package-macos uninstall uninstall-keep-data

# Build variables
VERSION ?= dev
BINARY_NAME := smarthome-hub
BUILD_DIR := ./build
DIST_DIR := ./dist

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
BLUE := \033[0;34m
NC := \033[0m # No Color

help:
	@echo "$(BLUE)SmartHome Hub - Build Commands$(NC)"
	@echo ""
	@echo "$(GREEN)Setup & Development:$(NC)"
	@echo "  make setup          - Install dependencies"
	@echo "  make dev            - Run in development mode"
	@echo ""
	@echo "$(GREEN)Building:$(NC)"
	@echo "  make build          - Build for current platform"
	@echo "  make build-all      - Build for all platforms (linux, windows, darwin)"
	@echo "  make build-linux    - Build for Linux (amd64, arm64)"
	@echo "  make build-windows  - Build for Windows (amd64, arm64)"
	@echo "  make build-macos    - Build for macOS (amd64, arm64)"
	@echo ""
	@echo "$(GREEN)Release & Distribution:$(NC)"
	@echo "  make release        - Create release with goreleaser"
	@echo "  make dist           - Build distribution packages"
	@echo "  make package-macos  - Build a macOS installer package (.pkg)"
	@echo "  make install        - Build and install the macOS package"
	@echo "  make uninstall      - Uninstall on macOS and remove app data"
	@echo "  make uninstall-keep-data - Uninstall on macOS but keep app data"
	@echo ""
	@echo "$(GREEN)Maintenance:$(NC)"
	@echo "  make test           - Run tests"
	@echo "  make clean          - Clean build artifacts"
	@echo "  make fmt            - Format code"

setup:
	@echo "$(BLUE)Setting up project...$(NC)"
	@go mod download
	@go mod tidy
	@echo "$(GREEN)✓ Setup complete$(NC)"

dev:
	@echo "$(BLUE)Running in development mode...$(NC)"
	@go run ./cmd/launcher

build: clean
	@echo "$(BLUE)Building $(BINARY_NAME)...$(NC)"
	@mkdir -p $(BUILD_DIR)
	@go build \
		-ldflags="-s -w -X main.version=$(VERSION)" \
		-o $(BUILD_DIR)/$(BINARY_NAME) \
		./cmd/launcher
	@echo "$(GREEN)✓ Built to $(BUILD_DIR)/$(BINARY_NAME)$(NC)"

build-linux:
	@echo "$(BLUE)Building for Linux...$(NC)"
	@mkdir -p $(BUILD_DIR)/linux
	@GOOS=linux GOARCH=amd64 CGO_ENABLED=1 go build \
		-ldflags="-s -w -X main.version=$(VERSION)" \
		-o $(BUILD_DIR)/linux/$(BINARY_NAME)-amd64 \
		./cmd/launcher
	@GOOS=linux GOARCH=arm64 CGO_ENABLED=1 go build \
		-ldflags="-s -w -X main.version=$(VERSION)" \
		-o $(BUILD_DIR)/linux/$(BINARY_NAME)-arm64 \
		./cmd/launcher
	@echo "$(GREEN)✓ Built for Linux$(NC)"

build-windows:
	@echo "$(BLUE)Building for Windows...$(NC)"
	@mkdir -p $(BUILD_DIR)/windows
	@GOOS=windows GOARCH=amd64 CGO_ENABLED=1 go build \
		-ldflags="-s -w -X main.version=$(VERSION)" \
		-o $(BUILD_DIR)/windows/$(BINARY_NAME)-amd64.exe \
		./cmd/launcher
	@GOOS=windows GOARCH=arm64 CGO_ENABLED=1 go build \
		-ldflags="-s -w -X main.version=$(VERSION)" \
		-o $(BUILD_DIR)/windows/$(BINARY_NAME)-arm64.exe \
		./cmd/launcher
	@echo "$(GREEN)✓ Built for Windows$(NC)"

build-macos:
	@echo "$(BLUE)Building for macOS...$(NC)"
	@mkdir -p $(BUILD_DIR)/macos
	@GOOS=darwin GOARCH=amd64 CGO_ENABLED=1 go build \
		-ldflags="-s -w -X main.version=$(VERSION)" \
		-o $(BUILD_DIR)/macos/$(BINARY_NAME)-amd64 \
		./cmd/launcher
	@GOOS=darwin GOARCH=arm64 CGO_ENABLED=1 go build \
		-ldflags="-s -w -X main.version=$(VERSION)" \
		-o $(BUILD_DIR)/macos/$(BINARY_NAME)-arm64 \
		./cmd/launcher
	@echo "$(GREEN)✓ Built for macOS$(NC)"

build-all: build-linux build-windows build-macos
	@echo "$(GREEN)✓ Built for all platforms$(NC)"

release:
	@echo "$(BLUE)Creating release with goreleaser...$(NC)"
	@goreleaser release --clean --config ../.goreleaser.yaml
	@echo "$(GREEN)✓ Release created$(NC)"

dist: clean build-all
	@echo "$(BLUE)Creating distribution packages...$(NC)"
	@mkdir -p $(DIST_DIR)
	@tar -czf $(DIST_DIR)/$(BINARY_NAME)-linux-amd64.tar.gz -C $(BUILD_DIR)/linux $(BINARY_NAME)-amd64
	@tar -czf $(DIST_DIR)/$(BINARY_NAME)-linux-arm64.tar.gz -C $(BUILD_DIR)/linux $(BINARY_NAME)-arm64
	@tar -czf $(DIST_DIR)/$(BINARY_NAME)-macos-amd64.tar.gz -C $(BUILD_DIR)/macos $(BINARY_NAME)-amd64
	@tar -czf $(DIST_DIR)/$(BINARY_NAME)-macos-arm64.tar.gz -C $(BUILD_DIR)/macos $(BINARY_NAME)-arm64
	@echo "$(GREEN)✓ Distribution packages created in $(DIST_DIR)$(NC)"

package-macos:
	@echo "$(BLUE)Building macOS package...$(NC)"
	@../scripts/package-macos.sh

install: package-macos
	@echo "$(BLUE)Installing macOS package...$(NC)"
	sudo installer -pkg $(BUILD_DIR)/SmartHome.Hub.pkg -target /
	@echo "$(GREEN)✓ Installed package$(NC)"

uninstall:
	@../scripts/uninstall-macos.sh

uninstall-keep-data:
	@../scripts/uninstall-macos.sh --keep-data

test:
	@echo "$(BLUE)Running tests...$(NC)"
	@go test -v ./...
	@echo "$(GREEN)✓ Tests passed$(NC)"

fmt:
	@echo "$(BLUE)Formatting code...$(NC)"
	@go fmt ./...
	@echo "$(GREEN)✓ Code formatted$(NC)"

clean:
	@echo "$(BLUE)Cleaning build artifacts...$(NC)"
	@rm -rf $(BUILD_DIR) $(DIST_DIR)
	@echo "$(GREEN)✓ Cleaned$(NC)"
