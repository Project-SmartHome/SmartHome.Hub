package main

import (
	"context"
	"errors"
	"net/http"
	"smarthome/hub/core/logger"
	hub "smarthome/hub/pkg/hub"
)

// @title SmartHome Hub API
// @version 1.0
// @description Smart Home Hub Backend API
// @host localhost:8080
// @BasePath /

// @securityDefinitions.apikey BearerAuth
// @in header
// @name Authorization
func main() {
	server, err := hub.New(hub.Config{})
	if err != nil {
		logger.Init("hub")
		logger.Log.Fatal("%s", err.Error())
	}

	if err := server.Run(context.Background()); err != nil && !errors.Is(err, http.ErrServerClosed) {
		logger.Log.Fatal("%s", err.Error())
	}
}
