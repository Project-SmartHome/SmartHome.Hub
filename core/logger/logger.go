package logger

import (
	"fmt"
	"math/rand"
	"os"
	"time"
)

type Logger struct {
	service string
	color   string
}

var Log *Logger

// ANSI colors
var colors = []string{
	"\033[31m", // red
	"\033[32m", // green
	"\033[33m", // yellow
	"\033[34m", // blue
	"\033[35m", // magenta
	"\033[36m", // cyan
}

const reset = "\033[0m"

func Init(service string) {
	rand.Seed(time.Now().UnixNano())

	color := colors[rand.Intn(len(colors))]

	Log = &Logger{
		service: service,
		color:   color,
	}
}

// internal formatter
func (l *Logger) log(level string, msg string, args ...interface{}) {
	timestamp := time.Now().Format("2006/01/02 15:04:05")
	message := fmt.Sprintf(msg, args...)

	fmt.Printf("%s[%s]%s %s %s %s\n",
		l.color,
		l.service,
		reset,
		timestamp,
		level,
		message,
	)
}

// public methods
func (l *Logger) Info(msg string, args ...interface{}) {
	l.log("INFO", msg, args...)
}

func (l *Logger) Error(msg string, args ...interface{}) {
	l.log("ERROR", msg, args...)
}

func (l *Logger) Fatal(msg string, args ...interface{}) {
	l.log("FATAL", msg, args...)
	os.Exit(1)
}

// // Gin Middleware
// func GinLogger() gin.HandlerFunc {
// 	return func(c *gin.Context) {
// 		start := time.Now()

// 		c.Next()

// 		duration := time.Since(start)

// 		status := c.Writer.Status()
// 		method := c.Request.Method
// 		path := c.Request.URL.Path

// 		if status >= 500 {
// 			Log.Error("%s %s %d %v", method, path, status, duration)
// 		} else {
// 			Log.Info("%s %s %d %v", method, path, status, duration)
// 		}
// 	}
// }
