package main

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"smarthome/hub/core/logger"
	hub "smarthome/hub/pkg/hub"
)

var (
	version = "dev"
	author  = "madirajupranay"
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
	printBanner()

	server, err := hub.New(hub.Config{})
	if err != nil {
		logger.Init("hub")
		logger.Log.Fatal("%s", err.Error())
	}

	if err := server.Run(context.Background()); err != nil && !errors.Is(err, http.ErrServerClosed) {
		logger.Log.Fatal("%s", err.Error())
	}
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
