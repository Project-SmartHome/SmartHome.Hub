package auth

import (
	"database/sql"
	"errors"
)

var (
	ErrUserNotFound = errors.New("user not found")
	ErrUserExists   = errors.New("user already exists")
)

type AuthRepository struct {
	DB *sql.DB
}

func NewAuthRepository(
	db *sql.DB,
) *AuthRepository {

	return &AuthRepository{
		DB: db,
	}
}

func (r *AuthRepository) GetUserByUsername(
	username string,
) (*User, error) {

	query := `
		SELECT id, username, password_hash
		FROM users
		WHERE username = ?
	`

	var user User

	err := r.DB.QueryRow(
		query,
		username,
	).Scan(
		&user.ID,
		&user.Username,
		&user.PasswordHash,
	)

	if err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return nil, ErrUserNotFound
		}

		return nil, err
	}

	return &user, nil
}

func (r *AuthRepository) CreateUser(
	username string,
	passwordHash string,
) error {

	query := `
		INSERT OR IGNORE INTO users (
			username,
			password_hash
		)
		VALUES (?, ?)
	`

	result, err := r.DB.Exec(
		query,
		username,
		passwordHash,
	)

	if err != nil {
		return err
	}

	rows, err := result.RowsAffected()
	if err != nil {
		return err
	}

	if rows == 0 {
		return ErrUserExists
	}

	return nil
}
