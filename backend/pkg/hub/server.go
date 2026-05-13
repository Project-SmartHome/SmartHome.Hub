package hub

import (
	"context"
	"database/sql"
	"encoding/json"
	"net/http"
	"os"
	"path/filepath"
	"smarthome/hub/core/logger"
	"smarthome/hub/core/middleware"
	"smarthome/hub/internal/auth"
	"smarthome/hub/internal/database"
	"smarthome/hub/pkg/updater"
	"strings"
	"time"

	_ "smarthome/hub/docs"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/cors"
	httpSwagger "github.com/swaggo/http-swagger"
)

type Config struct {
	Addr                    string
	DatabasePath            string
	AllowedOrigins          []string
	UpdateOwner             string
	UpdateRepo              string
	UpdateManifestAssetName string
	UpdateGitHubToken       string
	UpdateRoot              string
	UpdateConfigFile        string
	CurrentVersion          string
	AppEnv                  string
	DebugMode               bool
	EnableUpdateCheck       bool
	UpdateAutoApply         bool
}

type Server struct {
	Config        Config
	DB            *sql.DB
	Router        http.Handler
	UpdateChecker *updater.Checker
}

const (
	defaultUpdateOwner             = "Project-SmartHome"
	defaultUpdateRepo              = "SmartHome.Hub"
	defaultUpdateManifestAssetName = "update-manifest.json"
	defaultUpdateRoot              = "."
)

var Version = "dev"

func New(config Config) (*Server, error) {
	config = withDefaults(config)

	logger.Init("hub")

	db, err := database.Open(config.DatabasePath)
	if err != nil {
		return nil, err
	}

	database.DB = db

	var checker *updater.Checker
	if config.UpdateOwner != "" && config.UpdateRepo != "" {
		checker = &updater.Checker{
			Source: updater.GitHubSource{
				Owner:             config.UpdateOwner,
				Repo:              config.UpdateRepo,
				ManifestAssetName: config.UpdateManifestAssetName,
				APIToken:          config.UpdateGitHubToken,
			},
			Client: updater.Client{
				Root: config.UpdateRoot,
			},
			CurrentVersion: config.CurrentVersion,
			AutoApply:      config.UpdateAutoApply,
		}
	}

	router := NewRouter(db, config, checker)

	return &Server{
		Config:        config,
		DB:            db,
		Router:        router,
		UpdateChecker: checker,
	}, nil
}

func NewRouter(db *sql.DB, config Config, checker *updater.Checker) http.Handler {
	config = withDefaults(config)

	authRepo := auth.NewAuthRepository(db)
	authService := auth.NewAuthService(authRepo)
	authHandler := auth.NewAuthHandler(authService)

	router := chi.NewRouter()

	router.Use(middleware.LoggerMiddleware)
	router.Use(cors.Handler(cors.Options{
		AllowedOrigins: config.AllowedOrigins,
		AllowedMethods: []string{
			"GET",
			"POST",
			"PUT",
			"DELETE",
			"OPTIONS",
		},
		AllowedHeaders: []string{
			"Accept",
			"Authorization",
			"Content-Type",
			"X-CSRF-Token",
		},
		ExposedHeaders: []string{
			"Link",
		},
		AllowCredentials: true,
		MaxAge:           300,
	}))

	authHandler.RegisterRoutes(router)

	router.Get(
		"/swagger/*",
		httpSwagger.WrapHandler,
	)

	router.Get("/health", func(
		w http.ResponseWriter,
		r *http.Request,
	) {
		w.Write([]byte("OK"))
	})

	if checker != nil {
		router.Get("/update/check", handleUpdateCheck(checker))
		router.Post("/update/apply", handleUpdateApply(checker))
	}

	spa, err := spaHandler()
	if err != nil {
		logger.Log.Fatal("failed to create static frontend handler: %s", err.Error())
	}
	router.Handle("/*", spa)

	return router
}

func (s *Server) Run(ctx context.Context) error {
	if reason := s.updateCheckSkipReason(); reason != "" {
		logger.Log.Info("update check skipped: %s", reason)
	} else {
		logger.Log.Info("checking for updates")
		go s.checkForUpdates(context.Background())
	}

	httpServer := &http.Server{
		Addr:    s.Config.Addr,
		Handler: s.Router,
	}

	errs := make(chan error, 1)
	go func() {
		logger.Log.Info("Server running on %s", s.Config.Addr)
		errs <- httpServer.ListenAndServe()
	}()

	select {
	case <-ctx.Done():
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()
		if err := httpServer.Shutdown(shutdownCtx); err != nil {
			return err
		}
		return ctx.Err()
	case err := <-errs:
		if err == http.ErrServerClosed {
			return nil
		}
		return err
	}
}

func (s *Server) updateCheckSkipReason() string {
	if s.UpdateChecker == nil {
		return "github owner/repo not configured"
	}
	if !s.Config.EnableUpdateCheck {
		return "disabled"
	}
	if s.Config.DebugMode {
		return "debug mode enabled"
	}

	return ""
}

func (s *Server) checkForUpdates(ctx context.Context) {
	defer func() {
		if r := recover(); r != nil {
			logger.Log.Error("update check panicked: %v", r)
		}
	}()

	info, changes, err := s.UpdateChecker.CheckAndApply(ctx)
	if err != nil {
		logger.Log.Error("update check failed: %s", err.Error())
		return
	}

	if len(changes) > 0 {
		logger.Log.Info("update applied: %d changes, new version %s", len(changes), info.LatestVersion)
		return
	}

	if info.Available {
		logger.Log.Info("update available but not applied: %s", info.LatestVersion)
		return
	}

	logger.Log.Info("no update available")
}

func handleUpdateCheck(checker *updater.Checker) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		info, err := checker.Check(r.Context())
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(info)
	}
}

func handleUpdateApply(checker *updater.Checker) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		info, changes, err := checker.CheckAndApply(r.Context())
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}

		response := struct {
			updater.UpdateInfo
			Changes int `json:"changes"`
		}{
			UpdateInfo: info,
			Changes:    len(changes),
		}

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(response)
	}
}

func withDefaults(config Config) Config {
	if config.Addr == "" {
		config.Addr = ":8080"
	}

	if config.DatabasePath == "" {
		config.DatabasePath = os.Getenv("DATABASE_PATH")
	}
	if config.DatabasePath == "" {
		config.DatabasePath = defaultDatabasePath()
	}

	if len(config.AllowedOrigins) == 0 {
		config.AllowedOrigins = []string{
			"http://localhost:4200",
			"http://localhost:8100",
		}
	}

	if config.UpdateOwner == "" {
		config.UpdateOwner = os.Getenv("UPDATE_GITHUB_OWNER")
	}
	if config.UpdateOwner == "" {
		config.UpdateOwner = defaultUpdateOwner
	}
	if config.UpdateRepo == "" {
		config.UpdateRepo = os.Getenv("UPDATE_GITHUB_REPO")
	}
	if config.UpdateRepo == "" {
		config.UpdateRepo = defaultUpdateRepo
	}
	if config.UpdateManifestAssetName == "" {
		config.UpdateManifestAssetName = os.Getenv("UPDATE_GITHUB_MANIFEST_ASSET_NAME")
	}
	if config.UpdateManifestAssetName == "" {
		config.UpdateManifestAssetName = defaultUpdateManifestAssetName
	}
	if config.UpdateGitHubToken == "" {
		config.UpdateGitHubToken = os.Getenv("UPDATE_GITHUB_TOKEN")
	}
	if config.UpdateRoot == "" {
		config.UpdateRoot = os.Getenv("UPDATE_ROOT")
	}
	if config.UpdateRoot == "" {
		config.UpdateRoot = defaultUpdateRoot
	}
	if !config.EnableUpdateCheck {
		config.EnableUpdateCheck = envBool(os.Getenv("ENABLE_UPDATE_CHECK"))
	}
	if !config.EnableUpdateCheck {
		config.EnableUpdateCheck = true
	}
	if !config.UpdateAutoApply {
		config.UpdateAutoApply = envBool(os.Getenv("UPDATE_AUTO_APPLY"))
	}
	if !config.UpdateAutoApply {
		config.UpdateAutoApply = true
	}

	if config.CurrentVersion == "" {
		config.CurrentVersion = Version
	}

	return config
}

func defaultDatabasePath() string {
	if path := os.Getenv("DATABASE_PATH"); path != "" {
		return path
	}

	if dir, err := os.UserConfigDir(); err == nil {
		return filepath.Join(dir, "SmartHome.Hub", "hub.db")
	}

	return filepath.Join(os.TempDir(), "smarthome-hub", "hub.db")
}

func envBool(value string) bool {
	return strings.EqualFold(value, "true") || value == "1" || strings.EqualFold(value, "yes")
}
