package main

import (
	"log"
	"os"
	"path/filepath"
	"sort"
	"sync"
	"time"

	"github.com/google/uuid"
)

type SessionStatus string

const (
	StatusQueued    SessionStatus = "queued"
	StatusCompiling SessionStatus = "compiling"
	StatusDone      SessionStatus = "done"
	StatusError     SessionStatus = "error"
)

type Session struct {
	ID        string        `json:"session_id"`
	Status    SessionStatus `json:"status"`
	Errors    []string      `json:"errors,omitempty"`
	CreatedAt time.Time     `json:"created_at"`
	Dir       string        `json:"-"`
	Position  int           `json:"queue_position,omitempty"`
}

type SessionManager struct {
	mu       sync.RWMutex
	sessions map[string]*Session
	cfg      Config
}

func NewSessionManager(cfg Config) *SessionManager {
	sm := &SessionManager{
		sessions: make(map[string]*Session),
		cfg:      cfg,
	}
	go sm.cleanupLoop()
	return sm
}

func (sm *SessionManager) Create() (*Session, error) {
	sm.mu.Lock()
	defer sm.mu.Unlock()

	// Evict oldest if at capacity
	if len(sm.sessions) >= sm.cfg.MaxSessions {
		sm.evictOldest()
	}

	id := uuid.New().String()
	dir := filepath.Join(sm.cfg.SessionDir, id)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return nil, err
	}

	s := &Session{
		ID:        id,
		Status:    StatusQueued,
		CreatedAt: time.Now(),
		Dir:       dir,
	}
	sm.sessions[id] = s
	return s, nil
}

func (sm *SessionManager) Get(id string) *Session {
	sm.mu.RLock()
	defer sm.mu.RUnlock()
	return sm.sessions[id]
}

func (sm *SessionManager) UpdateStatus(id string, status SessionStatus, errors []string) {
	sm.mu.Lock()
	defer sm.mu.Unlock()
	if s, ok := sm.sessions[id]; ok {
		s.Status = status
		if errors != nil {
			s.Errors = errors
		}
	}
}

func (sm *SessionManager) evictOldest() {
	// Find the oldest session
	var oldest *Session
	for _, s := range sm.sessions {
		if oldest == nil || s.CreatedAt.Before(oldest.CreatedAt) {
			oldest = s
		}
	}
	if oldest != nil {
		log.Printf("Evicting session %s (created %v)", oldest.ID, oldest.CreatedAt)
		os.RemoveAll(oldest.Dir)
		delete(sm.sessions, oldest.ID)
	}
}

func (sm *SessionManager) cleanupLoop() {
	ticker := time.NewTicker(60 * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		sm.cleanup()
	}
}

func (sm *SessionManager) cleanup() {
	sm.mu.Lock()
	defer sm.mu.Unlock()

	ttl := time.Duration(sm.cfg.SessionTTL) * time.Second
	now := time.Now()
	var expired []string

	for id, s := range sm.sessions {
		if now.Sub(s.CreatedAt) > ttl {
			expired = append(expired, id)
		}
	}

	// Sort to remove oldest first
	sort.Slice(expired, func(i, j int) bool {
		return sm.sessions[expired[i]].CreatedAt.Before(sm.sessions[expired[j]].CreatedAt)
	})

	for _, id := range expired {
		s := sm.sessions[id]
		log.Printf("Cleaning up expired session %s", id)
		os.RemoveAll(s.Dir)
		delete(sm.sessions, id)
	}

	if len(expired) > 0 {
		log.Printf("Cleaned up %d expired sessions", len(expired))
	}
}
