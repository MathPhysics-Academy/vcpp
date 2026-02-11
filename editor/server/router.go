package main

import (
	"encoding/json"
	"io"
	"log"
	"net/http"
	"path/filepath"
	"strings"
)

type CompileRequest struct {
	Code string `json:"code"`
}

type CompileResponse struct {
	SessionID string `json:"session_id"`
}

type StatusResponse struct {
	Status   string   `json:"status"`
	Errors   []string `json:"errors,omitempty"`
	Position int      `json:"queue_position,omitempty"`
}

type Router struct {
	mux      *http.ServeMux
	cfg      Config
	compiler *Compiler
	sm       *SessionManager
}

func NewRouter(cfg Config, compiler *Compiler, sm *SessionManager) *Router {
	r := &Router{
		mux:      http.NewServeMux(),
		cfg:      cfg,
		compiler: compiler,
		sm:       sm,
	}
	r.setupRoutes()
	return r
}

func (r *Router) ServeHTTP(w http.ResponseWriter, req *http.Request) {
	// CORS headers
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
	w.Header().Set("Access-Control-Allow-Headers", "Content-Type")

	if req.Method == "OPTIONS" {
		w.WriteHeader(http.StatusOK)
		return
	}

	r.mux.ServeHTTP(w, req)
}

func (r *Router) setupRoutes() {
	r.mux.HandleFunc("GET /", r.handleIndex)
	r.mux.HandleFunc("POST /api/compile", r.handleCompile)
	r.mux.HandleFunc("GET /api/status/{id}", r.handleStatus)
	r.mux.Handle("GET /output/", http.StripPrefix("/output/", http.FileServer(http.Dir(r.cfg.SessionDir))))
	r.mux.Handle("GET /static/", http.StripPrefix("/static/", http.FileServer(http.Dir(r.cfg.FrontendDir))))
}

func (r *Router) handleIndex(w http.ResponseWriter, req *http.Request) {
	if req.URL.Path != "/" {
		// Check if it's a static file request
		http.ServeFile(w, req, filepath.Join(r.cfg.FrontendDir, req.URL.Path))
		return
	}
	http.ServeFile(w, req, filepath.Join(r.cfg.FrontendDir, "index.html"))
}

func (r *Router) handleCompile(w http.ResponseWriter, req *http.Request) {
	// Limit request body size
	req.Body = http.MaxBytesReader(w, req.Body, int64(r.cfg.MaxSourceBytes+1024))

	body, err := io.ReadAll(req.Body)
	if err != nil {
		http.Error(w, "Request body too large", http.StatusRequestEntityTooLarge)
		return
	}

	var compileReq CompileRequest
	if err := json.Unmarshal(body, &compileReq); err != nil {
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		return
	}

	if strings.TrimSpace(compileReq.Code) == "" {
		http.Error(w, "Empty source code", http.StatusBadRequest)
		return
	}

	session, err := r.compiler.Compile(compileReq.Code)
	if err != nil {
		log.Printf("Compile error: %v", err)
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(CompileResponse{SessionID: session.ID})
}

func (r *Router) handleStatus(w http.ResponseWriter, req *http.Request) {
	id := req.PathValue("id")
	if id == "" {
		http.Error(w, "Missing session ID", http.StatusBadRequest)
		return
	}

	session := r.sm.Get(id)
	if session == nil {
		http.Error(w, "Session not found", http.StatusNotFound)
		return
	}

	resp := StatusResponse{
		Status:   string(session.Status),
		Errors:   session.Errors,
		Position: session.Position,
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}
