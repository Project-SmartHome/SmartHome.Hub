package auth

import (
	"encoding/json"
	"mime"
	"net/http"
	"strings"

	"github.com/go-chi/chi/v5"
)

type AuthHandler struct {
	Service *AuthService
}

func NewAuthHandler(s *AuthService) *AuthHandler {
	return &AuthHandler{Service: s}
}

func (h *AuthHandler) RegisterRoutes(r chi.Router) {
	r.Route("/api/auth", func(r chi.Router) {
		r.Post("/login", h.Login)
		r.Post("/register", h.Register)
	})
}

func decodeJSON(w http.ResponseWriter, r *http.Request, dst interface{}) bool {
	r.Body = http.MaxBytesReader(w, r.Body, 1<<20)
	decoder := json.NewDecoder(r.Body)
	decoder.DisallowUnknownFields()

	if err := decoder.Decode(dst); err != nil {
		writeError(w, http.StatusBadRequest, "invalid request body")
		return false
	}

	return true
}

func decodeLoginRequest(w http.ResponseWriter, r *http.Request, req *LoginRequest) bool {
	contentType, _, err := mime.ParseMediaType(r.Header.Get("Content-Type"))
	if err != nil && r.Header.Get("Content-Type") != "" {
		writeError(w, http.StatusBadRequest, "invalid content type")
		return false
	}

	if contentType == "application/x-www-form-urlencoded" {
		r.Body = http.MaxBytesReader(w, r.Body, 1<<20)
		if err := r.ParseForm(); err != nil {
			writeError(w, http.StatusBadRequest, "invalid form body")
			return false
		}

		req.Username = r.PostForm.Get("username")
		req.Password = r.PostForm.Get("password")
		return true
	}

	return decodeJSON(w, r, req)
}

func writeJSON(w http.ResponseWriter, status int, value interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(value)
}

func writeError(w http.ResponseWriter, status int, message string) {
	writeJSON(w, status, ErrorResponse{Error: message})
}

// Login godoc
//
// @Summary Login user
// @Description Authenticate user and return JWT token
// @Tags auth
// @Accept json
// @Accept x-www-form-urlencoded
// @Produce json
// @Param request body LoginRequest true "Login payload"
// @Param username formData string true "Username"
// @Param password formData string true "Password"
// @Success 200 {object} AuthResponse
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Router /api/auth/login [post]
func (h *AuthHandler) Login(
	w http.ResponseWriter,
	r *http.Request,
) {

	var req LoginRequest

	if !decodeLoginRequest(w, r, &req) {
		return
	}

	req.Username = strings.TrimSpace(req.Username)

	err := h.Service.Authenticate(
		req.Username,
		req.Password,
	)

	if err != nil {
		writeError(w, http.StatusUnauthorized, "invalid username or password")
		return
	}

	token, err := GenerateToken(req.Username)
	if err != nil {
		writeError(w, http.StatusInternalServerError, "token error")
		return
	}

	writeJSON(w, http.StatusOK, AuthResponse{Token: token})
}

// Register godoc
//
// @Summary Register user
// @Description Create user account
// @Tags auth
// @Accept json
// @Produce json
// @Param request body RegisterRequest true "Register payload"
// @Success 201 {object} AuthResponse
// @Failure 400 {object} ErrorResponse
// @Router /api/auth/register [post]
func (h *AuthHandler) Register(
	w http.ResponseWriter,
	r *http.Request,
) {

	var req RegisterRequest

	if !decodeJSON(w, r, &req) {
		return
	}

	req.Username = strings.TrimSpace(req.Username)

	if err := RequireJWTSecret(); err != nil {
		writeError(w, http.StatusInternalServerError, "token error")
		return
	}

	err := h.Service.Register(
		req.Username,
		req.Password,
	)

	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	token, err := GenerateToken(
		req.Username,
	)

	if err != nil {
		writeError(w, http.StatusInternalServerError, "token error")
		return
	}

	writeJSON(
		w,
		http.StatusCreated,
		AuthResponse{
			Token: token,
		},
	)
}
