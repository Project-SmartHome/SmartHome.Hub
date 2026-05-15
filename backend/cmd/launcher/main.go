package main

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"smarthome/hub/core/logger"
	"smarthome/hub/internal/config"
	hub "smarthome/hub/pkg/hub"
	"syscall"
	"time"
)

var (
	version            = "dev"
	author             = "Project-SmartHome"
	errRestartRequired = errors.New("application updated: restart required")
)

func main() {
	printBanner()

	// Setup logger
	logger.Init("launcher")

	// Load configuration
	configPath := filepath.Join(".", "config.json")
	cfg, err := config.LoadConfig(configPath)
	if err != nil {
		logger.Log.Warn("Failed to load config: %s, using defaults", err.Error())
		cfg = config.DefaultConfig()
	}
	currentVersion := runtimeVersion()

	// Check for updates before starting server
	logger.Log.Info("Checking for application updates...")
	updateCtx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	err = checkAndApplyUpdates(updateCtx, cfg, currentVersion)
	cancel()
	if err != nil {
		if errors.Is(err, errRestartRequired) {
			logger.Log.Info("Restart required after update. Restarting application...")
			if err := restartApplication(); err != nil {
				logger.Log.Error("Failed to restart application: %s", err.Error())
				logger.Log.Info("Please restart the application manually.")
				return
			}
			return
		}
		logger.Log.Warn("Update check failed: %s", err.Error())
	}

	// Create and start the hub server
	logger.Log.Info("Starting SmartHome Hub server on %s", cfg.Addr)
	server, err := hub.New(hub.Config{
		Addr:                   cfg.Addr,
		DatabasePath:           cfg.DatabasePath,
		UpdateOwner:            cfg.UpdateOwner,
		UpdateRepo:             cfg.UpdateRepo,
		UpdateGitHubToken:      cfg.UpdateGitHubToken,
		CurrentVersion:         currentVersion,
		AppEnv:                 cfg.AppEnv,
		DebugMode:              cfg.DebugMode,
		EnableUpdateCheck:      cfg.EnableUpdateCheck,
		UpdateAutoApply:        cfg.UpdateAutoApply,
		SkipStartupUpdateCheck: true,
	})
	if err != nil {
		logger.Log.Fatal("Failed to create server: %s", err.Error())
	}

	// Run server in a goroutine
	serverCtx, serverCancel := context.WithCancel(context.Background())
	errChan := make(chan error, 1)

	go func() {
		if err := server.Run(serverCtx); err != nil && !errors.Is(err, http.ErrServerClosed) {
			errChan <- err
		}
	}()

	// Setup graceful shutdown
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt, syscall.SIGTERM)

	logger.Log.Info("SmartHome Hub is running. Press Ctrl+C to stop.")

	select {
	case err := <-errChan:
		logger.Log.Error("Server error: %s", err.Error())
		serverCancel()
	case sig := <-sigChan:
		logger.Log.Info("Received signal: %v. Shutting down...", sig)
		serverCancel()
		// Wait a bit for graceful shutdown
		time.Sleep(time.Second)
	}
}

func runtimeVersion() string {
	if version == "" {
		return "dev"
	}

	return version
}

func checkAndApplyUpdates(ctx context.Context, cfg config.Config, currentVersion string) error {
	if !cfg.EnableUpdateCheck {
		logger.Log.Debug("Update check disabled")
		return nil
	}

	if cfg.UpdateOwner == "" || cfg.UpdateRepo == "" {
		logger.Log.Warn("Update check skipped: missing owner or repo configuration")
		return nil
	}

	logger.Log.Info("Checking for updates from %s/%s", cfg.UpdateOwner, cfg.UpdateRepo)

	// Check for updates
	release, err := hub.CheckForUpdates(ctx, cfg.UpdateOwner, cfg.UpdateRepo, cfg.UpdateGitHubToken)
	if err != nil {
		logger.Log.Warn("Failed to check for updates: %s", err.Error())
		return nil // Don't fail startup due to update check
	}

	if release.Version == "" {
		logger.Log.Debug("No releases found")
		return nil
	}

	logger.Log.Info("Latest version available: %s (current: %s)", release.Version, currentVersion)

	if release.Version == currentVersion {
		logger.Log.Debug("Already up to date")
		return nil
	}

	if !cfg.UpdateAutoApply {
		logger.Log.Info("Update available (v%s). Set updateAutoApply=true in config to auto-update.", release.Version)
		return nil
	}

	// Apply updates
	changes, err := hub.ApplyUpdates(ctx, release)
	if err != nil {
		logger.Log.Error("Failed to apply updates: %s", err.Error())
		return fmt.Errorf("update failed: %w", err)
	}

	if len(changes) == 0 {
		logger.Log.Info("Already up to date")
		return nil
	}

	logger.Log.Info("Applied %d updates successfully", len(changes))

	logger.Log.Info("Updates applied successfully. Please restart the application.")
	return errRestartRequired
}

func restartApplication() error {
	exePath, err := os.Executable()
	if err != nil {
		return fmt.Errorf("failed to get executable path: %w", err)
	}

	cwd, err := os.Getwd()
	if err != nil {
		return fmt.Errorf("failed to get working directory: %w", err)
	}

	cmd := exec.Command(exePath, os.Args[1:]...)
	cmd.Dir = cwd
	cmd.Env = os.Environ()
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Start(); err != nil {
		return fmt.Errorf("failed to start updated application: %w", err)
	}

	logger.Log.Info("Started updated application with pid %d", cmd.Process.Pid)
	return nil
}

func printBanner() {
	fmt.Printf(`
  ____                       _   _   _                         _   _       _     
 / ___| _ __ ___   __ _ _ __| |_| | | | ___  _ __ ___   ___  | | | |_   _| |__  
 \___ \| '_ ' _ \ / _' | '__| __| |_| |/ _ \| '_ ' _ \ / _ \ | |_| | | | | '_ \ 
  ___) | | | | | | (_| | |  | |_|  _  | (_) | | | | | |  __/_|  _  | |_| | |_) |
 |____/|_| |_| |_|\__,_|_|   \__|_| |_|\___/|_| |_| |_|\___(_)_| |_|\__,_|_.__/ 

 Author:  %s
 Version: %s

`, author, version)
}
