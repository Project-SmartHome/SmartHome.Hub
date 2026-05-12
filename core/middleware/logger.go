package middleware

import (
	"net/http"
	"smarthome/hub/core/logger"
	"time"
)

type responseWriter struct {
	http.ResponseWriter
	statusCode int
}

func (rw *responseWriter) WriteHeader(code int) {
	rw.statusCode = code
	rw.ResponseWriter.WriteHeader(code)
}

func LoggerMiddleware(
	next http.Handler,
) http.Handler {

	return http.HandlerFunc(func(
		w http.ResponseWriter,
		r *http.Request,
	) {

		start := time.Now()

		rw := &responseWriter{
			ResponseWriter: w,
			statusCode:     http.StatusOK,
		}

		next.ServeHTTP(rw, r)

		duration := time.Since(start)

		method := r.Method
		path := r.URL.Path
		status := rw.statusCode

		if status >= 500 {

			logger.Log.Error(
				"%s %s %d %v",
				method,
				path,
				status,
				duration,
			)

			return
		}

		logger.Log.Info(
			"%s %s %d %v",
			method,
			path,
			status,
			duration,
		)
	})
}
