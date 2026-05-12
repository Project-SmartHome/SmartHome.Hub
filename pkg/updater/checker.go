package updater

import (
	"context"
	"errors"
	"fmt"
)

type Checker struct {
	Source         GitHubSource
	Client         Client
	CurrentVersion string
	AutoApply      bool
}

type UpdateInfo struct {
	CurrentVersion      string `json:"currentVersion"`
	LatestVersion       string `json:"latestVersion,omitempty"`
	Available           bool   `json:"available"`
	RequiresBaseVersion string `json:"requiresBaseVersion,omitempty"`
	ReleaseName         string `json:"releaseName,omitempty"`
	ReleaseURL          string `json:"releaseUrl,omitempty"`
	ManifestURL         string `json:"manifestUrl,omitempty"`
}

func (c Checker) Check(ctx context.Context) (UpdateInfo, error) {
	if c.CurrentVersion == "" {
		return UpdateInfo{}, errors.New("current version is required")
	}

	release, err := c.Source.Latest(ctx)
	if err != nil {
		return UpdateInfo{}, err
	}

	info := c.buildInfo(release)
	return info, nil
}

func (c Checker) CheckAndApply(ctx context.Context) (UpdateInfo, []Change, error) {
	if c.CurrentVersion == "" {
		return UpdateInfo{}, nil, errors.New("current version is required")
	}

	release, err := c.Source.Latest(ctx)
	if err != nil {
		return UpdateInfo{}, nil, err
	}

	info := c.buildInfo(release)
	if !info.Available {
		return info, nil, nil
	}

	changes, err := c.Apply(ctx, release)
	if err != nil {
		return info, nil, err
	}

	return info, changes, nil
}

func (c Checker) LatestRelease(ctx context.Context) (GitHubRelease, error) {
	return c.Source.Latest(ctx)
}

func (c Checker) Apply(ctx context.Context, release GitHubRelease) ([]Change, error) {
	if c.CurrentVersion == "" {
		return nil, errors.New("current version is required")
	}

	if release.Manifest.BaseVersion != "" && release.Manifest.BaseVersion != c.CurrentVersion {
		return nil, fmt.Errorf("manifest requires baseVersion %s", release.Manifest.BaseVersion)
	}

	if c.Client.Root == "" {
		c.Client.Root = "."
	}

	return c.Client.Apply(ctx, release.Manifest)
}

func (c Checker) buildInfo(release GitHubRelease) UpdateInfo {
	info := UpdateInfo{
		CurrentVersion: c.CurrentVersion,
		LatestVersion:  release.Version,
		ReleaseName:    release.Name,
		ReleaseURL:     release.HTMLURL,
		ManifestURL:    release.ManifestURL,
	}

	if release.Manifest.BaseVersion != "" {
		info.RequiresBaseVersion = release.Manifest.BaseVersion
	}

	hasUpdate := release.Version != c.CurrentVersion
	compatible := release.Manifest.BaseVersion == "" || release.Manifest.BaseVersion == c.CurrentVersion
	info.Available = hasUpdate && compatible

	return info
}
