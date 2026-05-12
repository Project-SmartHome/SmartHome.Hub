go mod tidy

go run .

swag init -g ./hub.go

@Security BearerAuth

## Library usage

The server can be embedded from another Go program:

```go
server, err := hub.New(hub.Config{
	Addr:         ":8080",
	DatabasePath: "./data/hub.db",
})
if err != nil {
	log.Fatal(err)
}

log.Fatal(server.Run(context.Background()))
```

Import path:

```go
import hub "smarthome/hub/pkg/hub"
```

## Differential updates

`pkg/updater` applies file-level differential updates from a manifest. It only downloads and replaces files whose SHA-256 checksum differs from the installed copy.

To check GitHub Releases, upload an `update-manifest.json` asset to your latest release, then fetch it like this:

```go
source := updater.GitHubSource{
	Owner: "your-github-user-or-org",
	Repo:  "SmartHome.Hub",
	// APIToken: os.Getenv("GITHUB_TOKEN"), // optional for private repos or higher rate limits
}

release, err := source.Latest(context.Background())
if err != nil {
	log.Fatal(err)
}

client := updater.Client{
	Root:    "/path/to/install",
	BaseURL: "https://github.com/your-github-user-or-org/SmartHome.Hub/releases/download/v" + release.Version + "/",
}

changes, err := client.Apply(context.Background(), release.Manifest)
```

```json
{
  "version": "1.0.1",
  "baseVersion": "1.0.0",
  "files": [
    {
      "path": "hub",
      "url": "https://updates.example.com/1.0.1/hub",
      "sha256": "expected_sha256_hex",
      "size": 12345678,
      "mode": 493
    }
  ],
  "delete": ["old-file"]
}
```
