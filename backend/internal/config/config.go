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

	// Application settings
	AppEnv     string `json:"appEnv"`
	DebugMode  bool   `json:"debugMode"`
	FrontendFS bool   `json:"frontendFS"` // Whether to serve embedded frontend
}

// DefaultConfig returns default configuration
func DefaultConfig() Config {
	return Config{
		Addr:              ":8080",
		DatabasePath:      DefaultDatabasePath(),
		UpdateOwner:       "Project-SmartHome",
		UpdateRepo:        "SmartHome.Hub",
		EnableUpdateCheck: true,
		UpdateAutoApply:   true,
		AppEnv:            "production",
		DebugMode:         false,
		FrontendFS:        true,
	}
}

func DefaultDatabasePath() string {
	if dir, err := os.UserConfigDir(); err == nil {
		return filepath.Join(dir, "SmartHome.Hub", "hub.db")
	}

	return filepath.Join(os.TempDir(), "smarthome-hub", "hub.db")
}

// LoadConfig loads configuration from a JSON file or returns defaults
func LoadConfig(configPath string) (Config, error) {
	cfg := DefaultConfig()

	if configPath == "" {
		return cfg, nil
	}

	// If file doesn't exist, run entirely from defaults.
	if _, err := os.Stat(configPath); os.IsNotExist(err) {
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
