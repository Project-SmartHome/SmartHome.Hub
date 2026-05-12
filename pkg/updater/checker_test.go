package updater

import (
	"context"
	"net/http"
	"os"
	"path/filepath"
	"testing"
)

func TestCheckerCheckReturnsAvailableUpdate(t *testing.T) {
	client := &http.Client{
		Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
			switch r.URL.Path {
			case "/repos/acme/hub/releases/latest":
				return jsonResponse(`{
					"tag_name": "v1.2.0",
					"name": "Release 1.2.0",
					"html_url": "https://github.com/acme/hub/releases/tag/v1.2.0",
					"assets": [
						{
							"name": "update-manifest.json",
							"browser_download_url": "https://uploads.example.com/download/update-manifest.json"
						}
					]
				}`), nil
			case "/download/update-manifest.json":
				return jsonResponse(`{
					"version": "1.2.0",
					"files": [
						{
							"path": "hub",
							"url": "file:///tmp/hub",
							"sha256": "` + sum("hub") + `"
						}
					]
				}`), nil
			default:
				t.Fatalf("unexpected request path %q", r.URL.Path)
				return nil, nil
			}
		}),
	}

	source := GitHubSource{
		Owner:      "acme",
		Repo:       "hub",
		HTTPClient: client,
	}

	checker := Checker{
		Source:         source,
		Client:         Client{Root: "."},
		CurrentVersion: "1.1.0",
	}

	info, err := checker.Check(context.Background())
	if err != nil {
		t.Fatal(err)
	}

	if !info.Available {
		t.Fatal("expected update to be available")
	}
}

func TestCheckerApplyRejectsBaseVersionMismatch(t *testing.T) {
	checker := Checker{
		Client:         Client{Root: "."},
		CurrentVersion: "1.0.0",
	}

	_, err := checker.Apply(context.Background(), GitHubRelease{Manifest: Manifest{BaseVersion: "2.0.0"}})
	if err == nil {
		t.Fatal("expected base version mismatch error")
	}
}

func TestCheckerApplyDownloadsManifestFiles(t *testing.T) {
	root := t.TempDir()
	filePath := filepath.Join(t.TempDir(), "new.txt")
	if err := os.WriteFile(filePath, []byte("new"), 0644); err != nil {
		t.Fatal(err)
	}

	checker := Checker{
		Client:         Client{Root: root},
		CurrentVersion: "1.0.0",
	}

	manifest := Manifest{
		Files: []ManifestFile{
			{
				Path:   "app.txt",
				URL:    "file://" + filePath,
				SHA256: sum("new"),
				Size:   3,
			},
		},
	}

	changes, err := checker.Client.Apply(context.Background(), manifest)
	if err != nil {
		t.Fatal(err)
	}

	if len(changes) != 1 {
		t.Fatalf("expected 1 change, got %d", len(changes))
	}
}
