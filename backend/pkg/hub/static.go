package hub

import (
	"embed"
	"errors"
	"io/fs"
	"net/http"
	"strings"
)

// StaticFS holds the embedded frontend assets
//go:embed frontend
var StaticFS embed.FS

func spaHandler() (http.Handler, error) {
	frontendFS, err := fs.Sub(StaticFS, "frontend")
	if err != nil {
		return nil, err
	}

	fileServer := http.FileServer(http.FS(frontendFS))

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		path := strings.TrimPrefix(r.URL.Path, "/")
		if path == "" {
			path = "index.html"
		}

		if _, err := fs.Stat(frontendFS, path); err != nil {
			if errors.Is(err, fs.ErrNotExist) {
				r.URL.Path = "/index.html"
			} else {
				http.Error(w, "Internal Server Error", http.StatusInternalServerError)
				return
			}
		} else {
			r.URL.Path = "/" + path
		}

		fileServer.ServeHTTP(w, r)
	}), nil
}
