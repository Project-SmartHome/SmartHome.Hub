go mod tidy

go run .

swag init -g ./hub.go

@Security BearerAuth

tar -xzf smarthome-hub_Darwin_arm64.tar.gz

chmod +x smarthome-hub

xattr -d com.apple.quarantine smarthome-hub