package auth

import (
	"errors"
	"strings"

	"golang.org/x/crypto/bcrypt"
)

var (
	ErrInvalidCredentials = errors.New("invalid username or password")
	ErrInvalidUsername    = errors.New("username must be between 3 and 64 characters")
	ErrInvalidPassword    = errors.New("password must be at least 8 characters")
)

type AuthService struct {
	Repo *AuthRepository
}

func NewAuthService(
	repo *AuthRepository,
) *AuthService {

	return &AuthService{
		Repo: repo,
	}
}

func (s *AuthService) Authenticate(
	username string,
	password string,
) error {

	username = strings.TrimSpace(username)
	if username == "" || password == "" {
		return ErrInvalidCredentials
	}

	user, err := s.Repo.GetUserByUsername(
		username,
	)

	if err != nil {
		return ErrInvalidCredentials
	}

	err = bcrypt.CompareHashAndPassword(
		[]byte(user.PasswordHash),
		[]byte(password),
	)

	if err != nil {
		return ErrInvalidCredentials
	}

	return nil
}

func (s *AuthService) Register(
	username string,
	password string,
) error {

	username = strings.TrimSpace(username)
	if len(username) < 3 || len(username) > 64 {
		return ErrInvalidUsername
	}

	if len(password) < 8 {
		return ErrInvalidPassword
	}

	hash, err := bcrypt.GenerateFromPassword(
		[]byte(password),
		bcrypt.DefaultCost,
	)

	if err != nil {
		return err
	}

	return s.Repo.CreateUser(
		username,
		string(hash),
	)
}
