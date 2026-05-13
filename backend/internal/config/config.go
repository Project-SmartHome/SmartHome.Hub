package config

import (
	"encoding/json"
	"os"
	"path/filepath"
)

// Config holds the application configuration
type Config struct {
	// Server settings
	Addr         string `json:"addr"`
	DatabasePath string `json:"databasePath"`

	// Update settings
	UpdateOwner       string `json:"updateOwner"`
	UpdateRepo        string `json:"updateRepo"`
	EnableUpdateCheck bool   `json:"enableUpdateCheck"`
	UpdateAutoApply   bool   `json:"updateAutoApply"`
	UpdateGitHubToken string `json:"updateGitHubToken"`
	CurrentVersion    string `json:"currentVersion"`

	// Application settings
	AppEnv     string `json:"appEnv"`
	DebugMode  bool   `json:"debugMode"`
	FrontendFS bool   `json:"frontendFS"` // Whether to serve embedded frontend
}

// DefaultConfig returns default configuration
func DefaultConfig() Config {
	return Config{
		Addr:              ":8080",
		DatabasePath:      "./data/hub.db",
		UpdateOwner:       "Project-SmartHome",
		UpdateRepo:        "SmartHome.Hub",
		EnableUpdateCheck: true,
		UpdateAutoApply:   true,
		CurrentVersion:    "dev",
		AppEnv:            "production",
		DebugMode:         false,
		FrontendFS:        true,
	}
}

// LoadConfig loads configuration from a JSON file or returns defaults
func LoadConfig(configPath string) (Config, error) {
	cfg := DefaultConfig()

	if configPath == "" {
		return cfg, nil
	}

	// Create config directory if it doesn't exist
	dir := filepath.Dir(configPath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return cfg, err
	}

	// If file doesn't exist, create it with defaults
	if _, err := os.Stat(configPath); os.IsNotExist(err) {
		if err := SaveConfig(configPath, cfg); err != nil {
			return cfg, err
		}
		return cfg, nil
	}

	// Load existing config
	data, err := os.ReadFile(configPath)
	if err != nil {
		return cfg, err
	}

	if err := json.Unmarshal(data, &cfg); err != nil {
		return cfg, err
	}

	return cfg, nil
}

// SaveConfig saves configuration to a JSON file
func SaveConfig(configPath string, cfg Config) error {
	data, err := json.MarshalIndent(cfg, "", "  ")
	if err != nil {
		return err
	}

	return os.WriteFile(configPath, data, 0644)
}
