package updater

import (
	"encoding/json"
	"io"
)

type Manifest struct {
	Version     string         `json:"version"`
	BaseVersion string         `json:"baseVersion,omitempty"`
	Files       []ManifestFile `json:"files"`
	Delete      []string       `json:"delete,omitempty"`
}

type ManifestFile struct {
	Path   string `json:"path"`
	URL    string `json:"url"`
	SHA256 string `json:"sha256"`
	Size   int64  `json:"size,omitempty"`
	Mode   uint32 `json:"mode,omitempty"`
	GOOS   string `json:"goos,omitempty"`
	GOARCH string `json:"goarch,omitempty"`
}

func DecodeManifest(r io.Reader) (Manifest, error) {
	var manifest Manifest
	if err := json.NewDecoder(r).Decode(&manifest); err != nil {
		return Manifest{}, err
	}

	return manifest, nil
}
