package auth

import (
	"errors"
	"os"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

var ErrMissingJWTSecret = errors.New("JWT_SECRET is not configured")

func signingSecret() ([]byte, error) {
	secret := os.Getenv("JWT_SECRET")
	if secret == "" {
		return nil, ErrMissingJWTSecret
	}

	return []byte(secret), nil
}

func RequireJWTSecret() error {
	_, err := signingSecret()
	return err
}

func GenerateToken(username string) (string, error) {

	secret, err := signingSecret()
	if err != nil {
		return "", err
	}

	now := time.Now()

	claims := Claims{
		Username: username,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(
				now.Add(24 * time.Hour),
			),
			IssuedAt: jwt.NewNumericDate(now),
		},
	}

	token := jwt.NewWithClaims(
		jwt.SigningMethodHS256,
		claims,
	)

	return token.SignedString(secret)
}
