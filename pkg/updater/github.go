package updater

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
)

var ErrManifestAssetNotFound = errors.New("manifest asset not found in GitHub release")

type GitHubSource struct {
	Owner             string
	Repo              string
	ManifestAssetName string
	APIToken          string
	APIBaseURL        string
	HTTPClient        *http.Client
}

type GitHubRelease struct {
	Version     string
	Name        string
	HTMLURL     string
	ManifestURL string
	Manifest    Manifest
}

func (s GitHubSource) Latest(ctx context.Context) (GitHubRelease, error) {
	if s.Owner == "" || s.Repo == "" {
		return GitHubRelease{}, errors.New("github owner and repo are required")
	}

	assetName := s.ManifestAssetName
	if assetName == "" {
		assetName = "update-manifest.json"
	}

	release, err := s.getLatestRelease(ctx)
	if err != nil {
		return GitHubRelease{}, err
	}

	for _, asset := range release.Assets {
		if asset.Name != assetName {
			continue
		}

		manifest, err := s.downloadManifest(ctx, asset.BrowserDownloadURL)
		if err != nil {
			return GitHubRelease{}, err
		}

		return GitHubRelease{
			Version:     strings.TrimPrefix(release.TagName, "v"),
			Name:        release.Name,
			HTMLURL:     release.HTMLURL,
			ManifestURL: asset.BrowserDownloadURL,
			Manifest:    manifest,
		}, nil
	}

	return GitHubRelease{}, fmt.Errorf("%w: %s", ErrManifestAssetNotFound, assetName)
}

func (s GitHubSource) getLatestRelease(ctx context.Context) (githubReleaseResponse, error) {
	baseURL := s.APIBaseURL
	if baseURL == "" {
		baseURL = "https://api.github.com"
	}

	endpoint, err := url.JoinPath(baseURL, "repos", s.Owner, s.Repo, "releases", "latest")
	if err != nil {
		return githubReleaseResponse{}, err
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return githubReleaseResponse{}, err
	}
	req.Header.Set("Accept", "application/vnd.github+json")
	req.Header.Set("X-GitHub-Api-Version", "2022-11-28")
	if s.APIToken != "" {
		req.Header.Set("Authorization", "Bearer "+s.APIToken)
	}

	resp, err := s.httpClient().Do(req)
	if err != nil {
		return githubReleaseResponse{}, err
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode > 299 {
		return githubReleaseResponse{}, fmt.Errorf("github latest release request failed with status %d", resp.StatusCode)
	}

	var release githubReleaseResponse
	if err := json.NewDecoder(resp.Body).Decode(&release); err != nil {
		return githubReleaseResponse{}, err
	}

	return release, nil
}

func (s GitHubSource) downloadManifest(ctx context.Context, manifestURL string) (Manifest, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, manifestURL, nil)
	if err != nil {
		return Manifest{}, err
	}
	if s.APIToken != "" {
		req.Header.Set("Authorization", "Bearer "+s.APIToken)
	}

	resp, err := s.httpClient().Do(req)
	if err != nil {
		return Manifest{}, err
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode > 299 {
		return Manifest{}, fmt.Errorf("github manifest download failed with status %d", resp.StatusCode)
	}

	return DecodeManifest(io.LimitReader(resp.Body, 10<<20))
}

func (s GitHubSource) httpClient() *http.Client {
	if s.HTTPClient != nil {
		return s.HTTPClient
	}

	return http.DefaultClient
}

type githubReleaseResponse struct {
	TagName string                `json:"tag_name"`
	Name    string                `json:"name"`
	HTMLURL string                `json:"html_url"`
	Assets  []githubAssetResponse `json:"assets"`
}

type githubAssetResponse struct {
	Name               string `json:"name"`
	BrowserDownloadURL string `json:"browser_download_url"`
}
