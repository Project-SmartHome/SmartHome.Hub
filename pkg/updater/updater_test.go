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

func sum(value string) string {
	hash := sha256.Sum256([]byte(value))
	return hex.EncodeToString(hash[:])
}
