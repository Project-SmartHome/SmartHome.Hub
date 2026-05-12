package updater

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"os"
	"path/filepath"
	"testing"
)

func TestApplyOnlyChangedFiles(t *testing.T) {
	root := t.TempDir()
	release := t.TempDir()

	currentPath := filepath.Join(root, "app.txt")
	if err := os.WriteFile(currentPath, []byte("old"), 0644); err != nil {
		t.Fatal(err)
	}

	newPath := filepath.Join(release, "app.txt")
	if err := os.WriteFile(newPath, []byte("new"), 0644); err != nil {
		t.Fatal(err)
	}

	unchangedPath := filepath.Join(root, "same.txt")
	if err := os.WriteFile(unchangedPath, []byte("same"), 0644); err != nil {
		t.Fatal(err)
	}

	manifest := Manifest{
		Version: "1.0.1",
		Files: []ManifestFile{
			{
				Path:   "app.txt",
				URL:    "file://" + newPath,
				SHA256: sum("new"),
				Size:   3,
			},
			{
				Path:   "same.txt",
				URL:    "file://" + unchangedPath,
				SHA256: sum("same"),
				Size:   4,
			},
		},
	}

	client := Client{Root: root}

	changes, err := client.Apply(context.Background(), manifest)
	if err != nil {
		t.Fatal(err)
	}

	if len(changes) != 1 {
		t.Fatalf("expected 1 change, got %d", len(changes))
	}

	updated, err := os.ReadFile(currentPath)
	if err != nil {
		t.Fatal(err)
	}

	if string(updated) != "new" {
		t.Fatalf("expected updated file content, got %q", updated)
	}
}

func TestRejectsUnsafePaths(t *testing.T) {
	client := Client{Root: t.TempDir()}

	_, err := client.Plan(Manifest{
		Files: []ManifestFile{
			{
				Path:   "../outside.txt",
				SHA256: sum("outside"),
			},
		},
	})

	if err == nil {
		t.Fatal("expected unsafe path error")
	}
}

func TestApplyOnlyCurrentPlatformFiles(t *testing.T) {
	root := t.TempDir()
	release := t.TempDir()

	linuxPath := filepath.Join(release, "linux")
	if err := os.WriteFile(linuxPath, []byte("linux"), 0644); err != nil {
		t.Fatal(err)
	}
	darwinPath := filepath.Join(release, "darwin")
	if err := os.WriteFile(darwinPath, []byte("darwin"), 0644); err != nil {
		t.Fatal(err)
	}

	manifest := Manifest{
		Version: "1.0.1",
		Files: []ManifestFile{
			{
				Path:   "SmartHome.Hub",
				URL:    "file://" + darwinPath,
				SHA256: sum("darwin"),
				Size:   6,
				GOOS:   "darwin",
				GOARCH: "arm64",
			},
			{
				Path:   "SmartHome.Hub",
				URL:    "file://" + linuxPath,
				SHA256: sum("linux"),
				Size:   5,
				GOOS:   "linux",
				GOARCH: "amd64",
			},
		},
	}

	client := Client{Root: root, GOOS: "linux", GOARCH: "amd64"}
	changes, err := client.Apply(context.Background(), manifest)
	if err != nil {
		t.Fatal(err)
	}

	if len(changes) != 1 {
		t.Fatalf("expected 1 change, got %d", len(changes))
	}

	updated, err := os.ReadFile(filepath.Join(root, "SmartHome.Hub"))
	if err != nil {
		t.Fatal(err)
	}

	if string(updated) != "linux" {
		t.Fatalf("expected linux file content, got %q", updated)
	}
}

func sum(value string) string {
	hash := sha256.Sum256([]byte(value))
	return hex.EncodeToString(hash[:])
}
