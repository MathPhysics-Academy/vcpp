// VCpp Live Editor — Main entry point
// Combines editor, compiler, output-frame, and examples logic in one file
// (no bundler needed for MVP)

'use strict';

// ============================================================================
// Examples
// ============================================================================

const EXAMPLES = {
  hello_sphere: {
    label: 'Hello Sphere',
    code: `module;
import std;
export module web_test;
import vcpp;
import vcpp.wgpu.web;

void update() {
    vcpp::scene.mark_dirty();
}

export int run_web_test() {
    using namespace vcpp;
    using namespace vcpp::wgpu::web;
    if (!init(scene, "#canvas")) return 1;

    scene.background(vec3{0.1, 0.1, 0.15});

    // Create a red sphere
    scene.add(sphere(pos=vec3{0, 0, 0}, radius=1.0, color=colors::red));

    // Add a green box nearby
    scene.add(box(pos=vec3{2.5, 0, 0}, length=1.0, height=1.0, width=1.0, color=colors::green));

    // Add a blue ellipsoid
    scene.add(ellipsoid(pos=vec3{-2.5, 0, 0}, length=2.0, height=0.5, width=0.5, color=colors::blue));

    run(update);
    return 0;
}
`
  },

  solar_system: {
    label: 'Solar System',
    code: `module;
import std;
export module web_test;
import vcpp;
import vcpp.wgpu.web;

namespace {
    double t = 0.0;
}

void update() {
    using namespace vcpp;
    t += 0.016;

    // Orbit the earth around the sun
    double earth_angle = t * 0.5;
    double earth_x = 5.0 * std::cos(earth_angle);
    double earth_z = 5.0 * std::sin(earth_angle);

    if (scene.m_spheres.size() >= 3) {
        scene.m_spheres[1].m_pos = vec3{earth_x, 0, earth_z};

        // Moon orbits earth
        double moon_angle = t * 2.0;
        double moon_x = earth_x + 1.2 * std::cos(moon_angle);
        double moon_z = earth_z + 1.2 * std::sin(moon_angle);
        scene.m_spheres[2].m_pos = vec3{moon_x, 0, moon_z};
    }

    scene.mark_dirty();
}

export int run_web_test() {
    using namespace vcpp;
    using namespace vcpp::wgpu::web;
    if (!init(scene, "#canvas")) return 1;

    scene.background(vec3{0.02, 0.02, 0.08});

    // Sun
    scene.add(sphere(pos=vec3{0, 0, 0}, radius=2.0, color=colors::yellow));
    // Earth
    scene.add(sphere(pos=vec3{5, 0, 0}, radius=0.8, color=colors::blue));
    // Moon
    scene.add(sphere(pos=vec3{6.2, 0, 0}, radius=0.3, color=vec3{0.6, 0.6, 0.6}));

    run(update);
    return 0;
}
`
  },

  bouncing_ball: {
    label: 'Bouncing Ball + Graph',
    code: `module;
import std;
export module web_test;
import vcpp;
import vcpp.wgpu.web;

namespace {
    double ball_y = 5.0;
    double ball_vy = 0.0;
    double sim_time = 0.0;
    std::size_t ball_idx = 0;

    constexpr double ball_g = 9.8;
    constexpr double ball_mass = 1.0;
    constexpr double restitution = 0.85;
    constexpr double floor_y = 0.0;
    constexpr double ball_radius = 0.5;
    constexpr double sim_dt = 0.01;
}

void update() {
    using namespace vcpp;

    // Physics
    ball_vy -= ball_g * sim_dt;
    ball_y += ball_vy * sim_dt;

    if (ball_y - ball_radius < floor_y) {
        ball_y = floor_y + ball_radius;
        ball_vy = -ball_vy * restitution;
    }

    if (ball_idx < scene.m_spheres.size()) {
        scene.m_spheres[ball_idx].m_pos = vec3{0, ball_y, 0};
    }

    // Energy calculations for graph
    double height = ball_y - ball_radius;
    double ke = 0.5 * ball_mass * ball_vy * ball_vy;
    double pe = ball_mass * ball_g * height;
    double total = ke + pe;

    static int frame_counter = 0;
    if (frame_counter % 3 == 0 && g_graphs.m_gcurves.size() >= 3) {
        g_graphs.m_gcurves[0].plot(sim_time, ke);
        g_graphs.m_gcurves[1].plot(sim_time, pe);
        g_graphs.m_gcurves[2].plot(sim_time, total);

        std::size_t graph_id = g_graphs.m_graphs[0].m_id;
        graph_bridge::queue_point(graph_id, g_graphs.m_gcurves[0].m_plot_id, sim_time, ke);
        graph_bridge::queue_point(graph_id, g_graphs.m_gcurves[1].m_plot_id, sim_time, pe);
        graph_bridge::queue_point(graph_id, g_graphs.m_gcurves[2].m_plot_id, sim_time, total);

        if (!g_graphs.m_graphs.empty())
            g_graphs.m_graphs[0].m_dirty = true;
    }
    frame_counter++;
    sim_time += sim_dt;

    scene.mark_dirty();
}

export int run_web_test() {
    using namespace vcpp;
    using namespace vcpp::wgpu::web;
    if (!init(scene, "#canvas")) return 1;

    scene.background(vec3{0.1, 0.1, 0.15});

    // Floor
    scene.add(box(pos=vec3{0, -0.5, 0}, length=10.0, height=1.0, width=10.0,
                  color=vec3{0.3, 0.3, 0.35}, shininess=0.8));

    // Ball
    scene.add(sphere(pos=vec3{0, ball_y, 0}, radius=ball_radius, color=colors::red));
    ball_idx = scene.m_spheres.size() - 1;

    // Energy graph
    auto energy_graph = graph(
        title="Bouncing Ball Energy",
        xtitle="Time (s)",
        ytitle="Energy (J)",
        width=300, height=200
    );
    g_graphs.add(energy_graph);

    auto ke_curve = gcurve(color=colors::red, prop::label="KE", width=2.0);
    auto pe_curve = gcurve(color=colors::blue, prop::label="PE", width=2.0);
    auto te_curve = gcurve(color=colors::green, prop::label="Total", width=2.0);

    g_graphs.add(ke_curve, g_graphs.m_graphs[0]);
    g_graphs.add(pe_curve, g_graphs.m_graphs[0]);
    g_graphs.add(te_curve, g_graphs.m_graphs[0]);

    graph_bridge::sync_all();

    run(update);
    return 0;
}
`
  }
};

// ============================================================================
// Editor Setup (Monaco)
// ============================================================================

let editor = null;
let monacoReady = false;

function initEditor() {
  return new Promise((resolve) => {
    require.config({
      paths: {
        vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.52.2/min/vs'
      }
    });

    require(['vs/editor/editor.main'], function (monaco) {
      // Register C++ language customizations for VCpp
      monaco.languages.registerCompletionItemProvider('cpp', {
        provideCompletionItems: function (model, position) {
          const word = model.getWordUntilPosition(position);
          const range = {
            startLineNumber: position.lineNumber,
            endLineNumber: position.lineNumber,
            startColumn: word.startColumn,
            endColumn: word.endColumn
          };

          const suggestions = [
            { label: 'sphere', kind: monaco.languages.CompletionItemKind.Function,
              insertText: 'sphere(pos=vec3{${1:0}, ${2:0}, ${3:0}}, radius=${4:1.0}, color=colors::${5:red})',
              insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
              range },
            { label: 'box', kind: monaco.languages.CompletionItemKind.Function,
              insertText: 'box(pos=vec3{${1:0}, ${2:0}, ${3:0}}, length=${4:1.0}, height=${5:1.0}, width=${6:1.0}, color=colors::${7:green})',
              insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
              range },
            { label: 'cylinder', kind: monaco.languages.CompletionItemKind.Function,
              insertText: 'cylinder(pos=vec3{${1:0}, ${2:0}, ${3:0}}, axis=vec3{${4:0}, ${5:1}, ${6:0}}, radius=${7:0.5}, color=colors::${8:blue})',
              insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
              range },
            { label: 'arrow', kind: monaco.languages.CompletionItemKind.Function,
              insertText: 'arrow(pos=vec3{${1:0}, ${2:0}, ${3:0}}, axis=vec3{${4:1}, ${5:0}, ${6:0}}, color=colors::${7:white})',
              insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
              range },
            { label: 'ellipsoid', kind: monaco.languages.CompletionItemKind.Function,
              insertText: 'ellipsoid(pos=vec3{${1:0}, ${2:0}, ${3:0}}, length=${4:2.0}, height=${5:1.0}, width=${6:1.0}, color=colors::${7:cyan})',
              insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
              range },
            { label: 'scene.add', kind: monaco.languages.CompletionItemKind.Method,
              insertText: 'scene.add(${1:});',
              insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
              range },
            { label: 'scene.mark_dirty', kind: monaco.languages.CompletionItemKind.Method,
              insertText: 'scene.mark_dirty();',
              range },
            { label: 'scene.clear', kind: monaco.languages.CompletionItemKind.Method,
              insertText: 'scene.clear();',
              range },
            { label: 'scene.background', kind: monaco.languages.CompletionItemKind.Method,
              insertText: 'scene.background(vec3{${1:0}, ${2:0}, ${3:0}});',
              insertTextRules: monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet,
              range },
          ];

          return { suggestions };
        }
      });

      editor = monaco.editor.create(document.getElementById('editor-container'), {
        value: EXAMPLES.hello_sphere.code,
        language: 'cpp',
        theme: 'vs-dark',
        fontSize: 14,
        fontFamily: "'Cascadia Code', 'Fira Code', 'Consolas', monospace",
        minimap: { enabled: false },
        scrollBeyondLastLine: false,
        automaticLayout: true,
        tabSize: 4,
        wordWrap: 'off',
        lineNumbers: 'on',
        renderLineHighlight: 'all',
        padding: { top: 8 },
      });

      // Ctrl+Enter to compile
      editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.Enter, () => {
        compileAndRun();
      });

      monacoReady = true;
      resolve(editor);
    });
  });
}

function setEditorMarkers(errors) {
  if (!editor || !window.monaco) return;

  const markers = [];
  for (const err of errors) {
    try {
      const parsed = JSON.parse(err);
      markers.push({
        severity: parsed.type === 'warning'
          ? window.monaco.MarkerSeverity.Warning
          : window.monaco.MarkerSeverity.Error,
        startLineNumber: parseInt(parsed.line) || 1,
        startColumn: parseInt(parsed.column) || 1,
        endLineNumber: parseInt(parsed.line) || 1,
        endColumn: 1000,
        message: parsed.message,
      });
    } catch {
      // Raw error string, can't place in editor
    }
  }

  window.monaco.editor.setModelMarkers(editor.getModel(), 'vcpp', markers);
}

function clearEditorMarkers() {
  if (!editor || !window.monaco) return;
  window.monaco.editor.setModelMarkers(editor.getModel(), 'vcpp', []);
}

// ============================================================================
// Compiler Client
// ============================================================================

let currentSessionId = null;
let pollTimer = null;

async function compileAndRun() {
  if (!editor) return;

  const code = editor.getValue();
  const runBtn = document.getElementById('run-btn');
  const statusText = document.getElementById('status-text');

  // Cancel any existing poll
  if (pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
  }

  // Clear previous errors
  clearEditorMarkers();
  hideErrorPanel();

  runBtn.disabled = true;
  runBtn.textContent = 'Compiling...';
  runBtn.classList.add('compiling');
  statusText.textContent = 'Submitting...';
  statusText.className = '';

  try {
    const resp = await fetch('/api/compile', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ code }),
    });

    if (!resp.ok) {
      const text = await resp.text();
      throw new Error(text);
    }

    const data = await resp.json();
    currentSessionId = data.session_id;
    statusText.textContent = 'Compiling...';

    // Start polling
    pollTimer = setInterval(() => pollStatus(data.session_id), 500);
  } catch (err) {
    statusText.textContent = 'Error: ' + err.message;
    statusText.className = 'error';
    runBtn.disabled = false;
    runBtn.textContent = 'Run';
    runBtn.classList.remove('compiling');
    showErrors([err.message]);
  }
}

async function pollStatus(sessionId) {
  const runBtn = document.getElementById('run-btn');
  const statusText = document.getElementById('status-text');

  try {
    const resp = await fetch(`/api/status/${sessionId}`);
    if (!resp.ok) throw new Error('Status check failed');

    const data = await resp.json();

    switch (data.status) {
      case 'queued':
        statusText.textContent = data.queue_position > 0
          ? `Queued (position ${data.queue_position})`
          : 'Queued...';
        break;

      case 'compiling':
        statusText.textContent = 'Compiling...';
        break;

      case 'done':
        clearInterval(pollTimer);
        pollTimer = null;
        statusText.textContent = 'Done!';
        statusText.className = 'success';
        runBtn.disabled = false;
        runBtn.textContent = 'Run';
        runBtn.classList.remove('compiling');
        loadOutput(sessionId);
        break;

      case 'error':
        clearInterval(pollTimer);
        pollTimer = null;
        statusText.textContent = 'Compilation failed';
        statusText.className = 'error';
        runBtn.disabled = false;
        runBtn.textContent = 'Run';
        runBtn.classList.remove('compiling');
        if (data.errors && data.errors.length > 0) {
          setEditorMarkers(data.errors);
          showErrors(data.errors);
        }
        break;
    }
  } catch (err) {
    clearInterval(pollTimer);
    pollTimer = null;
    statusText.textContent = 'Poll error';
    statusText.className = 'error';
    runBtn.disabled = false;
    runBtn.textContent = 'Run';
    runBtn.classList.remove('compiling');
  }
}

// ============================================================================
// Output Frame
// ============================================================================

function loadOutput(sessionId) {
  const frame = document.getElementById('output-frame');
  const placeholder = document.getElementById('output-placeholder');

  frame.style.display = 'block';
  placeholder.style.display = 'none';
  frame.src = `/output/${sessionId}/runner.html`;
}

// ============================================================================
// Error Panel
// ============================================================================

function showErrors(errors) {
  const panel = document.getElementById('error-panel');
  const messages = document.getElementById('error-messages');
  const title = document.getElementById('error-title');

  panel.classList.remove('hidden', 'collapsed');
  panel.classList.add('expanded');

  let hasErrors = false;
  let hasWarnings = false;
  let html = '';

  for (const err of errors) {
    try {
      const parsed = JSON.parse(err);
      const cls = parsed.type === 'warning' ? 'warning-line' : 'error-line';
      if (parsed.type === 'warning') hasWarnings = true;
      else hasErrors = true;
      html += `<span class="${cls}">Line ${parsed.line}:${parsed.column}: ${parsed.type}: ${parsed.message}</span>\n`;
    } catch {
      hasErrors = true;
      html += `<span class="error-line">${escapeHtml(err)}</span>\n`;
    }
  }

  messages.innerHTML = html;

  title.className = '';
  if (hasErrors) {
    title.textContent = `Compiler Output (${errors.length} error${errors.length > 1 ? 's' : ''})`;
    title.classList.add('has-errors');
  } else if (hasWarnings) {
    title.textContent = `Compiler Output (${errors.length} warning${errors.length > 1 ? 's' : ''})`;
    title.classList.add('has-warnings');
  }
}

function hideErrorPanel() {
  const panel = document.getElementById('error-panel');
  const messages = document.getElementById('error-messages');
  const title = document.getElementById('error-title');

  panel.classList.add('hidden');
  panel.classList.remove('expanded');
  messages.innerHTML = '';
  title.textContent = 'Compiler Output';
  title.className = '';
}

function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

// ============================================================================
// Divider (resize)
// ============================================================================

function initDivider() {
  const divider = document.getElementById('divider');
  const editorPane = document.getElementById('editor-pane');
  const outputPane = document.getElementById('output-pane');
  const workspace = document.getElementById('workspace');

  let isDragging = false;

  divider.addEventListener('mousedown', (e) => {
    isDragging = true;
    divider.classList.add('dragging');
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
    e.preventDefault();
  });

  document.addEventListener('mousemove', (e) => {
    if (!isDragging) return;

    const rect = workspace.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const totalWidth = rect.width;
    const dividerWidth = 6;

    const editorWidth = Math.max(200, Math.min(totalWidth - 200 - dividerWidth, x));
    const outputWidth = totalWidth - editorWidth - dividerWidth;

    editorPane.style.flex = 'none';
    editorPane.style.width = editorWidth + 'px';
    outputPane.style.flex = 'none';
    outputPane.style.width = outputWidth + 'px';
  });

  document.addEventListener('mouseup', () => {
    if (isDragging) {
      isDragging = false;
      divider.classList.remove('dragging');
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    }
  });
}

// ============================================================================
// Examples dropdown
// ============================================================================

function initExamples() {
  const select = document.getElementById('examples-select');

  for (const [key, example] of Object.entries(EXAMPLES)) {
    const option = document.createElement('option');
    option.value = key;
    option.textContent = example.label;
    select.appendChild(option);
  }

  select.addEventListener('change', () => {
    if (!select.value || !editor) return;
    const example = EXAMPLES[select.value];
    if (example) {
      editor.setValue(example.code);
      clearEditorMarkers();
      hideErrorPanel();
    }
    select.value = '';
  });
}

// ============================================================================
// Error panel toggle
// ============================================================================

function initErrorPanel() {
  const header = document.getElementById('error-header');
  const panel = document.getElementById('error-panel');

  header.addEventListener('click', () => {
    if (panel.classList.contains('expanded')) {
      panel.classList.remove('expanded');
      panel.classList.add('collapsed');
    } else {
      panel.classList.remove('collapsed');
      panel.classList.add('expanded');
    }
  });
}

// ============================================================================
// Init
// ============================================================================

document.addEventListener('DOMContentLoaded', () => {
  initDivider();
  initExamples();
  initErrorPanel();
  initEditor();

  document.getElementById('run-btn').addEventListener('click', compileAndRun);
});
