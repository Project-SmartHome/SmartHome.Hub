package updater

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

var (
	ErrUnsafePath       = errors.New("unsafe update path")
	ErrChecksumMismatch = errors.New("checksum mismatch")
)

type Operation string

const (
	OperationPut    Operation = "put"
	OperationDelete Operation = "delete"
)

type Change struct {
	Operation Operation
	Path      string
	FromHash  string
	ToHash    string
}

type Client struct {
	Root       string
	BaseURL    string
	HTTPClient *http.Client
	GOOS       string
	GOARCH     string
}

func (c Client) Plan(manifest Manifest) ([]Change, error) {
	var changes []Change

	for _, file := range manifest.Files {
		if !c.matchesPlatform(file) {
			continue
		}

		target, err := c.targetPath(file.Path)
		if err != nil {
			return nil, err
		}

		currentHash, err := fileSHA256(target)
		if err != nil && !errors.Is(err, os.ErrNotExist) {
			return nil, err
		}

		if currentHash == file.SHA256 {
			continue
		}

		changes = append(changes, Change{
			Operation: OperationPut,
			Path:      file.Path,
			FromHash:  currentHash,
			ToHash:    file.SHA256,
		})
	}

	for _, path := range manifest.Delete {
		target, err := c.targetPath(path)
		if err != nil {
			return nil, err
		}

		currentHash, err := fileSHA256(target)
		if errors.Is(err, os.ErrNotExist) {
			continue
		}
		if err != nil {
			return nil, err
		}

		changes = append(changes, Change{
			Operation: OperationDelete,
			Path:      path,
			FromHash:  currentHash,
		})
	}

	return changes, nil
}

func (c Client) Apply(ctx context.Context, manifest Manifest) ([]Change, error) {
	changes, err := c.Plan(manifest)
	if err != nil {
		return nil, err
	}

	for _, change := range changes {
		switch change.Operation {
		case OperationPut:
			file, ok := c.manifestFile(manifest, change.Path)
			if !ok {
				return nil, fmt.Errorf("missing manifest file for %s", change.Path)
			}

			if err := c.applyPut(ctx, file); err != nil {
				return nil, err
			}
		case OperationDelete:
			target, err := c.targetPath(change.Path)
			if err != nil {
				return nil, err
			}
			if err := os.Remove(target); err != nil && !errors.Is(err, os.ErrNotExist) {
				return nil, err
			}
		default:
			return nil, fmt.Errorf("unknown operation %s", change.Operation)
		}
	}

	return changes, nil
}

func (c Client) applyPut(ctx context.Context, file ManifestFile) error {
	target, err := c.targetPath(file.Path)
	if err != nil {
		return err
	}

	if err := os.MkdirAll(filepath.Dir(target), 0755); err != nil {
		return err
	}

	tmp, err := os.CreateTemp(filepath.Dir(target), ".update-*")
	if err != nil {
		return err
	}
	tmpPath := tmp.Name()
	defer os.Remove(tmpPath)

	body, err := c.open(ctx, file.URL)
	if err != nil {
		tmp.Close()
		return err
	}
	defer body.Close()

	hash := sha256.New()
	written, err := io.Copy(tmp, io.TeeReader(body, hash))
	if err != nil {
		tmp.Close()
		return err
	}

	if file.Size > 0 && written != file.Size {
		tmp.Close()
		return fmt.Errorf("size mismatch for %s: got %d, want %d", file.Path, written, file.Size)
	}

	actual := hex.EncodeToString(hash.Sum(nil))
	if actual != file.SHA256 {
		tmp.Close()
		return fmt.Errorf("%w for %s: got %s, want %s", ErrChecksumMismatch, file.Path, actual, file.SHA256)
	}

	mode := os.FileMode(0644)
	if file.Mode != 0 {
		mode = os.FileMode(file.Mode)
	}

	if err := tmp.Chmod(mode); err != nil {
		tmp.Close()
		return err
	}

	if err := tmp.Close(); err != nil {
		return err
	}

	return os.Rename(tmpPath, target)
}

func (c Client) open(ctx context.Context, rawURL string) (io.ReadCloser, error) {
	resolved, err := c.resolveURL(rawURL)
	if err != nil {
		return nil, err
	}

	if resolved.Scheme == "file" || resolved.Scheme == "" {
		return os.Open(resolved.Path)
	}

	client := c.HTTPClient
	if client == nil {
		client = http.DefaultClient
	}

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, resolved.String(), nil)
	if err != nil {
		return nil, err
	}

	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}

	if resp.StatusCode < 200 || resp.StatusCode > 299 {
		resp.Body.Close()
		return nil, fmt.Errorf("download %s failed with status %d", resolved.String(), resp.StatusCode)
	}

	return resp.Body, nil
}

func (c Client) resolveURL(rawURL string) (*url.URL, error) {
	parsed, err := url.Parse(rawURL)
	if err != nil {
		return nil, err
	}

	if parsed.IsAbs() || c.BaseURL == "" {
		return parsed, nil
	}

	base, err := url.Parse(c.BaseURL)
	if err != nil {
		return nil, err
	}

	return base.ResolveReference(parsed), nil
}

func (c Client) targetPath(path string) (string, error) {
	clean := filepath.Clean(path)
	if clean == "." || filepath.IsAbs(clean) || strings.HasPrefix(clean, ".."+string(filepath.Separator)) || clean == ".." {
		return "", fmt.Errorf("%w: %s", ErrUnsafePath, path)
	}

	root := c.Root
	if root == "" {
		root = "."
	}

	target := filepath.Join(root, clean)
	rootAbs, err := filepath.Abs(root)
	if err != nil {
		return "", err
	}
	targetAbs, err := filepath.Abs(target)
	if err != nil {
		return "", err
	}

	if targetAbs != rootAbs && !strings.HasPrefix(targetAbs, rootAbs+string(filepath.Separator)) {
		return "", fmt.Errorf("%w: %s", ErrUnsafePath, path)
	}

	return targetAbs, nil
}

func fileSHA256(path string) (string, error) {
	file, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer file.Close()

	hash := sha256.New()
	if _, err := io.Copy(hash, file); err != nil {
		return "", err
	}

	return hex.EncodeToString(hash.Sum(nil)), nil
}

func (c Client) manifestFile(manifest Manifest, path string) (ManifestFile, bool) {
	for _, file := range manifest.Files {
		if file.Path == path && c.matchesPlatform(file) {
			return file, true
		}
	}

	return ManifestFile{}, false
}

func (c Client) matchesPlatform(file ManifestFile) bool {
	targetGOOS := c.GOOS
	if targetGOOS == "" {
		targetGOOS = runtime.GOOS
	}
	targetGOARCH := c.GOARCH
	if targetGOARCH == "" {
		targetGOARCH = runtime.GOARCH
	}

	return (file.GOOS == "" || file.GOOS == targetGOOS) &&
		(file.GOARCH == "" || file.GOARCH == targetGOARCH)
}
