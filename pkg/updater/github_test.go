package updater

import (
	"context"
	"io"
	"net/http"
	"strings"
	"testing"
)

func TestGitHubSourceLatestDownloadsManifestAsset(t *testing.T) {
	client := &http.Client{
		Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
			if got := r.Header.Get("Authorization"); got != "Bearer token" {
				t.Fatalf("expected auth token header, got %q", got)
			}

			switch r.URL.Path {
			case "/repos/acme/hub/releases/latest":
				return jsonResponse(`{
					"tag_name": "v1.2.3",
					"name": "Release 1.2.3",
					"html_url": "https://github.com/acme/hub/releases/tag/v1.2.3",
					"assets": [
						{
							"name": "update-manifest.json",
							"browser_download_url": "https://uploads.example.com/download/update-manifest.json"
						}
					]
				}`), nil
			case "/download/update-manifest.json":
				return jsonResponse(`{
					"version": "1.2.3",
					"files": [
						{
							"path": "hub",
							"url": "hub",
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
		APIToken:   "token",
		APIBaseURL: "https://api.example.com",
		HTTPClient: client,
	}

	release, err := source.Latest(context.Background())
	if err != nil {
		t.Fatal(err)
	}

	if release.Version != "1.2.3" {
		t.Fatalf("expected version 1.2.3, got %q", release.Version)
	}

	if len(release.Manifest.Files) != 1 {
		t.Fatalf("expected 1 manifest file, got %d", len(release.Manifest.Files))
	}
}

func TestGitHubSourceLatestRequiresManifestAsset(t *testing.T) {
	client := &http.Client{
		Transport: roundTripFunc(func(r *http.Request) (*http.Response, error) {
			return jsonResponse(`{
				"tag_name": "v1.2.3",
				"assets": []
			}`), nil
		}),
	}

	source := GitHubSource{
		Owner:      "acme",
		Repo:       "hub",
		APIBaseURL: "https://api.example.com",
		HTTPClient: client,
	}

	_, err := source.Latest(context.Background())
	if err == nil {
		t.Fatal("expected missing manifest asset error")
	}
}

type roundTripFunc func(*http.Request) (*http.Response, error)

func (f roundTripFunc) RoundTrip(r *http.Request) (*http.Response, error) {
	return f(r)
}

func jsonResponse(body string) *http.Response {
	return &http.Response{
		StatusCode: http.StatusOK,
		Header: http.Header{
			"Content-Type": []string{"application/json"},
		},
		Body: io.NopCloser(strings.NewReader(body)),
	}
}
