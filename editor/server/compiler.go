package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"time"
)

type CompileError struct {
	Line    int    `json:"line"`
	Column  int    `json:"column"`
	Message string `json:"message"`
	Type    string `json:"type"` // "error" or "warning"
}

type Compiler struct {
	cfg  Config
	sm   *SessionManager
	pool *WorkerPool
}

func NewCompiler(cfg Config, sm *SessionManager, pool *WorkerPool) *Compiler {
	return &Compiler{cfg: cfg, sm: sm, pool: pool}
}

func (c *Compiler) Compile(code string) (*Session, error) {
	// Validate source size
	if len(code) > c.cfg.MaxSourceBytes {
		return nil, fmt.Errorf("source code exceeds maximum size of %d bytes", c.cfg.MaxSourceBytes)
	}

	// Basic sanity check
	if !strings.Contains(code, "export module web_test") {
		return nil, fmt.Errorf("source must contain 'export module web_test;'")
	}
	if !strings.Contains(code, "run_web_test") {
		return nil, fmt.Errorf("source must export 'run_web_test()' function")
	}

	// Create session
	session, err := c.sm.Create()
	if err != nil {
		return nil, fmt.Errorf("failed to create session: %w", err)
	}

	// Write user code
	userFile := filepath.Join(session.Dir, "user_code.cppm")
	if err := os.WriteFile(userFile, []byte(code), 0644); err != nil {
		return nil, fmt.Errorf("failed to write user code: %w", err)
	}

	// Submit compilation job
	pos := c.pool.Submit(Job{
		SessionID: session.ID,
		Fn: func() {
			c.doCompile(session)
		},
	})
	session.Position = pos

	return session, nil
}

func (c *Compiler) doCompile(session *Session) {
	c.sm.UpdateStatus(session.ID, StatusCompiling, nil)

	ctx, cancel := context.WithTimeout(context.Background(),
		time.Duration(c.cfg.CompileTimeout)*time.Second)
	defer cancel()

	// Step 1: Compile user module
	log.Printf("[%s] Step 1: Compiling user module...", session.ID[:8])
	start := time.Now()

	modmapFile := filepath.Join(c.cfg.PrebuiltDir, "templates", "user.modmap")
	userFile := filepath.Join(session.Dir, "user_code.cppm")
	userObj := filepath.Join(session.Dir, "user_code.o")

	compileArgs := []string{
		"-std=c++23",
		"@" + modmapFile,
		"--use-port=emdawnwebgpu",
		"-c", userFile,
		"-o", userObj,
	}

	var compileStdout, compileStderr bytes.Buffer
	compileCmd := exec.CommandContext(ctx, "em++", compileArgs...)
	compileCmd.Stdout = &compileStdout
	compileCmd.Stderr = &compileStderr
	compileCmd.Dir = c.cfg.PrebuiltDir // Run from prebuilt dir for relative paths

	if err := compileCmd.Run(); err != nil {
		errors := parseCompilerErrors(compileStderr.String())
		if len(errors) == 0 {
			errMsg := compileStderr.String()
			if errMsg == "" {
				errMsg = err.Error()
			}
			errors = []string{errMsg}
		}
		log.Printf("[%s] Compilation failed (%v): %s", session.ID[:8], time.Since(start), compileStderr.String())
		c.sm.UpdateStatus(session.ID, StatusError, errors)
		return
	}
	log.Printf("[%s] Step 1 complete (%v)", session.ID[:8], time.Since(start))

	// Step 2: Link
	log.Printf("[%s] Step 2: Linking...", session.ID[:8])
	linkStart := time.Now()

	outputJS := filepath.Join(session.Dir, "output.js")

	linkArgs := c.buildLinkArgs(userObj, outputJS)

	var linkStdout, linkStderr bytes.Buffer
	linkCmd := exec.CommandContext(ctx, "em++", linkArgs...)
	linkCmd.Stdout = &linkStdout
	linkCmd.Stderr = &linkStderr

	if err := linkCmd.Run(); err != nil {
		errMsg := linkStderr.String()
		if errMsg == "" {
			errMsg = err.Error()
		}
		log.Printf("[%s] Link failed (%v): %s", session.ID[:8], time.Since(linkStart), errMsg)
		c.sm.UpdateStatus(session.ID, StatusError, []string{"Link error: " + errMsg})
		return
	}
	log.Printf("[%s] Step 2 complete (%v)", session.ID[:8], time.Since(linkStart))

	// Copy support files to session output
	c.copyTemplateFiles(session)

	log.Printf("[%s] Compilation complete (total %v)", session.ID[:8], time.Since(start))
	c.sm.UpdateStatus(session.ID, StatusDone, nil)
}

func (c *Compiler) buildLinkArgs(userObj, outputJS string) []string {
	objDir := filepath.Join(c.cfg.PrebuiltDir, "obj")
	libDir := filepath.Join(c.cfg.PrebuiltDir, "lib")
	shellFile := filepath.Join(c.cfg.PrebuiltDir, "templates", "shell.html")

	// All prebuilt object files
	objects := []string{
		filepath.Join(objDir, "main.cpp.o"),
		filepath.Join(objDir, "vcpp.cppm.o"),
		filepath.Join(objDir, "vcpp-vec.cppm.o"),
		filepath.Join(objDir, "vcpp-color.cppm.o"),
		filepath.Join(objDir, "vcpp-props.cppm.o"),
		filepath.Join(objDir, "vcpp-traits.cppm.o"),
		filepath.Join(objDir, "vcpp-render-types.cppm.o"),
		filepath.Join(objDir, "vcpp-render-resources.cppm.o"),
		filepath.Join(objDir, "vcpp-mesh.cppm.o"),
		filepath.Join(objDir, "vcpp-render-interface.cppm.o"),
		filepath.Join(objDir, "vcpp-object-base.cppm.o"),
		filepath.Join(objDir, "vcpp-objects.cppm.o"),
		filepath.Join(objDir, "vcpp-shapes.cppm.o"),
		filepath.Join(objDir, "vcpp-text-glyphs.cppm.o"),
		filepath.Join(objDir, "vcpp-label-bridge.cppm.o"),
		filepath.Join(objDir, "vcpp-scene.cppm.o"),
		filepath.Join(objDir, "vcpp-loop.cppm.o"),
		filepath.Join(objDir, "vcpp-coro.cppm.o"),
		filepath.Join(objDir, "vcpp-input.cppm.o"),
		filepath.Join(objDir, "vcpp-graph-base.cppm.o"),
		filepath.Join(objDir, "vcpp-graph-objects.cppm.o"),
		filepath.Join(objDir, "vcpp-graph-bridge.cppm.o"),
		filepath.Join(objDir, "vcpp_wgpu_web.cppm.o"),
	}

	args := make([]string, 0, len(objects)+20)
	args = append(args, objects...)
	args = append(args, userObj)

	// Static libraries
	args = append(args,
		filepath.Join(libDir, "liblam_symbols.a"),
		filepath.Join(libDir, "liblam_linearalgebra.a"),
		filepath.Join(libDir, "liblam_concepts.a"),
		filepath.Join(libDir, "lib__cmake_cxx23.a"),
	)

	// Link flags
	args = append(args,
		"--use-port=emdawnwebgpu",
		"-sASYNCIFY",
		"-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']",
		"-sALLOW_MEMORY_GROWTH=1",
		"--shell-file="+shellFile,
		"-o", outputJS,
	)

	return args
}

func (c *Compiler) copyTemplateFiles(session *Session) {
	templateDir := c.cfg.TemplateDir

	copyFile := func(name string) {
		data, err := os.ReadFile(filepath.Join(templateDir, name))
		if err != nil {
			log.Printf("[%s] Warning: failed to read template %s: %v", session.ID[:8], name, err)
			return
		}
		if err := os.WriteFile(filepath.Join(session.Dir, name), data, 0644); err != nil {
			log.Printf("[%s] Warning: failed to write template %s: %v", session.ID[:8], name, err)
		}
	}

	copyFile("runner.html")
	copyFile("graph_instance.js")
	copyFile("graph_manager.js")
}

// parseCompilerErrors extracts structured error messages from em++/clang output
func parseCompilerErrors(stderr string) []string {
	if stderr == "" {
		return nil
	}

	// Match clang error/warning format: file:line:col: error/warning: message
	re := regexp.MustCompile(`(?m)^[^:]+:(\d+):(\d+):\s+(error|warning):\s+(.+)$`)
	matches := re.FindAllStringSubmatch(stderr, -1)

	if len(matches) == 0 {
		// Return the raw stderr as a single error
		trimmed := strings.TrimSpace(stderr)
		if trimmed != "" {
			return []string{trimmed}
		}
		return nil
	}

	var errors []string
	for _, m := range matches {
		// Format: "line:col: type: message"
		entry := map[string]string{
			"line":    m[1],
			"column":  m[2],
			"type":    m[3],
			"message": m[4],
		}
		b, _ := json.Marshal(entry)
		errors = append(errors, string(b))
	}
	return errors
}
