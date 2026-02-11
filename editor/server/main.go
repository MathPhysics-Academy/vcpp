package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"strconv"
)

type Config struct {
	Port           int
	Workers        int
	SessionTTL     int // seconds
	MaxSessions    int
	MaxSourceBytes int
	CompileTimeout int // seconds
	PrebuiltDir    string
	SessionDir     string
	FrontendDir    string
	TemplateDir    string
}

func loadConfig() Config {
	cfg := Config{
		Port:           8080,
		Workers:        4,
		SessionTTL:     1800, // 30 minutes
		MaxSessions:    100,
		MaxSourceBytes: 102400, // 100KB
		CompileTimeout: 60,
		PrebuiltDir:    "/prebuilt/cache",
		SessionDir:     "/tmp/sessions",
		FrontendDir:    "/app/frontend",
		TemplateDir:    "/app/templates",
	}

	if v := os.Getenv("PORT"); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			cfg.Port = n
		}
	}
	if v := os.Getenv("WORKERS"); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			cfg.Workers = n
		}
	}
	if v := os.Getenv("SESSION_TTL"); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			cfg.SessionTTL = n
		}
	}
	if v := os.Getenv("MAX_SESSIONS"); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			cfg.MaxSessions = n
		}
	}
	if v := os.Getenv("COMPILE_TIMEOUT"); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			cfg.CompileTimeout = n
		}
	}
	if v := os.Getenv("PREBUILT_DIR"); v != "" {
		cfg.PrebuiltDir = v
	}
	if v := os.Getenv("SESSION_DIR"); v != "" {
		cfg.SessionDir = v
	}
	if v := os.Getenv("FRONTEND_DIR"); v != "" {
		cfg.FrontendDir = v
	}
	if v := os.Getenv("TEMPLATE_DIR"); v != "" {
		cfg.TemplateDir = v
	}

	return cfg
}

func main() {
	cfg := loadConfig()

	log.Printf("VCpp Live Editor Server starting...")
	log.Printf("  Port: %d", cfg.Port)
	log.Printf("  Workers: %d", cfg.Workers)
	log.Printf("  Session TTL: %ds", cfg.SessionTTL)
	log.Printf("  Prebuilt dir: %s", cfg.PrebuiltDir)

	// Ensure session directory exists
	if err := os.MkdirAll(cfg.SessionDir, 0755); err != nil {
		log.Fatalf("Failed to create session directory: %v", err)
	}

	sm := NewSessionManager(cfg)
	pool := NewWorkerPool(cfg.Workers)
	compiler := NewCompiler(cfg, sm, pool)

	router := NewRouter(cfg, compiler, sm)

	addr := fmt.Sprintf(":%d", cfg.Port)
	log.Printf("Listening on %s", addr)

	server := &http.Server{
		Addr:    addr,
		Handler: router,
	}

	if err := server.ListenAndServe(); err != nil {
		log.Fatalf("Server failed: %v", err)
	}
}
