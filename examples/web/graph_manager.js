/**
 * graph_manager.js - DOM management for vcpp 2D graphs
 */

class VcppGraphManager {
  constructor() {
    this.graphs = new Map();
    this.container = null;
    this.resizeHandle = null;
    this.panelWidth = 320;
    this.isResizing = false;
    this.pendingRedraws = new Set();
    this.rafId = null;
    this.isPaused = false;

    this.init();
  }

  init() {
    // Wait for DOM to be ready
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => this.setup());
    } else {
      this.setup();
    }
  }

  setup() {
    this.container = document.getElementById('graph-container');
    this.resizeHandle = document.getElementById('graph-resize-handle');

    // Manual resize handle events
    if (this.resizeHandle) {
      this.resizeHandle.addEventListener('mousedown', (e) => {
        this.isResizing = true;
        this.resizeHandle.classList.add('dragging');
        document.body.style.cursor = 'ew-resize';
        document.body.style.userSelect = 'none';
        e.preventDefault();
      });

      document.addEventListener('mousemove', (e) => {
        if (!this.isResizing) return;
        const mainContainer = document.getElementById('container');
        if (!mainContainer) return;

        const containerRect = mainContainer.getBoundingClientRect();
        // Calculate new width: distance from mouse to right edge
        const newWidth = containerRect.right - e.clientX;

        // Clamp between min and max
        const clampedWidth = Math.max(150, Math.min(containerRect.width * 0.6, newWidth));

        console.log(`[Resize] mouse=${e.clientX}, right=${containerRect.right}, newWidth=${newWidth}, clamped=${clampedWidth}`);

        this.panelWidth = clampedWidth;
        if (this.container) {
          this.container.style.width = clampedWidth + 'px';
        }
        this.updateAllGraphSizes();
      });

      document.addEventListener('mouseup', () => {
        if (this.isResizing) {
          this.isResizing = false;
          this.resizeHandle.classList.remove('dragging');
          document.body.style.cursor = '';
          document.body.style.userSelect = '';
        }
      });
    }

    // Poll for size changes (backup for window resize, etc.)
    this.lastWidth = 0;
    setInterval(() => {
      if (this.container && this.container.style.display !== 'none' && !this.isResizing) {
        const currentWidth = this.container.clientWidth;
        if (currentWidth > 0 && Math.abs(currentWidth - this.lastWidth) > 5) {
          this.lastWidth = currentWidth;
          this.panelWidth = currentWidth;
          this.updateAllGraphSizes();
        }
      }
    }, 500);

    console.log('[GraphManager] Initialized with size polling');
  }

  showPanel() {
    if (this.container) {
      this.container.style.display = 'flex';
    }
    if (this.resizeHandle) {
      this.resizeHandle.style.display = 'block';
    }
  }

  hidePanel() {
    if (this.container) {
      this.container.style.display = 'none';
    }
    if (this.resizeHandle) {
      this.resizeHandle.style.display = 'none';
    }
  }

  updateAllGraphSizes() {
    if (!this.container) return;
    const containerWidth = this.container.clientWidth - 20;
    if (containerWidth < 50) return;

    for (const [id, graph] of this.graphs) {
      // Always use the ORIGINAL dimensions from C++ for aspect ratio
      const origW = 300;  // Fixed original width
      const origH = 200;  // Fixed original height
      const aspectRatio = origH / origW;

      const newWidth = Math.round(containerWidth);
      const newHeight = Math.round(containerWidth * aspectRatio);

      console.log(`[GraphResize] id=${id} containerWidth=${containerWidth} -> ${newWidth}x${newHeight}`);

      // Always resize (don't skip)
      graph.width = newWidth;
      graph.height = newHeight;

      // Resize canvas using individual properties (not cssText)
      const dpr = window.devicePixelRatio || 1;
      graph.canvas.width = newWidth * dpr;
      graph.canvas.height = newHeight * dpr;
      graph.canvas.style.display = 'block';
      graph.canvas.style.width = newWidth + 'px';
      graph.canvas.style.height = newHeight + 'px';

      // Fresh context
      graph.ctx = graph.canvas.getContext('2d');
      graph.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

      // Update margins
      graph.margin = {
        top: graph.title ? 40 : 20,
        right: 20,
        bottom: graph.xtitle ? 50 : 35,
        left: graph.ytitle ? 60 : 50
      };

      // Resize wrapper using individual properties
      const wrapper = document.getElementById(`graph-wrapper-${id}`);
      if (wrapper) {
        const oldHeight = wrapper.style.height;
        // Clear any conflicting styles first
        wrapper.style.minHeight = '';
        wrapper.style.maxHeight = '';
        // Set dimensions explicitly
        wrapper.style.width = newWidth + 'px';
        wrapper.style.height = newHeight + 'px';
        console.log(`[GraphResize] wrapper height: ${oldHeight} -> ${wrapper.style.height}, computed: ${getComputedStyle(wrapper).height}`);
      }

      graph.draw();
    }
  }

  createGraph(id, width, height, background, foreground, title, xtitle, ytitle) {
    console.log(`[GraphManager] Creating graph ${id}: "${title}"`);

    this.showPanel();

    const wrapper = document.createElement('div');
    wrapper.className = 'graph-wrapper';
    wrapper.id = `graph-wrapper-${id}`;

    const availableWidth = this.panelWidth - 20;
    const scale = availableWidth / width;
    const scaledWidth = Math.round(width * scale);
    const scaledHeight = Math.round(height * scale);

    // Set styles individually to avoid cssText issues
    wrapper.style.width = scaledWidth + 'px';
    wrapper.style.height = scaledHeight + 'px';
    wrapper.style.background = `rgba(${Math.round(background[0]*255)}, ${Math.round(background[1]*255)}, ${Math.round(background[2]*255)}, 0.95)`;
    wrapper.style.borderRadius = '8px';
    wrapper.style.overflow = 'hidden';
    wrapper.style.boxShadow = '0 2px 8px rgba(0,0,0,0.3)';
    wrapper.style.flexShrink = '0';
    // Ensure no min/max constraints
    wrapper.style.minHeight = 'none';
    wrapper.style.maxHeight = 'none';

    const canvas = document.createElement('canvas');
    canvas.id = `graph-canvas-${id}`;
    canvas.style.display = 'block';
    wrapper.appendChild(canvas);
    this.container.appendChild(wrapper);

    const instance = new GraphInstance(id, canvas, {
      width: scaledWidth,
      height: scaledHeight,
      background,
      foreground,
      title,
      xtitle,
      ytitle
    });

    instance.originalWidth = width;
    instance.originalHeight = height;

    this.graphs.set(id, instance);
    instance.draw();

    return instance;
  }

  createPlot(graphId, plotId, type, color, label, widthOrRadius, delta) {
    const graph = this.graphs.get(graphId);
    if (!graph) return;
    graph.createPlot(plotId, type, color, label, widthOrRadius, delta);
  }

  addPoint(graphId, plotId, x, y) {
    const graph = this.graphs.get(graphId);
    if (!graph) return;
    graph.addPoint(plotId, x, y);
    this.scheduleRedraw(graphId);
  }

  addPointsBatch(graphId, plotId, points) {
    const graph = this.graphs.get(graphId);
    if (!graph) return;
    graph.addPointsBatch(plotId, points);
    this.scheduleRedraw(graphId);
  }

  updateBounds(graphId, xmin, xmax, ymin, ymax) {
    const graph = this.graphs.get(graphId);
    if (!graph) return;
    graph.updateBounds(xmin, xmax, ymin, ymax);
  }

  redraw(graphId) {
    const graph = this.graphs.get(graphId);
    if (!graph) return;
    graph.draw();
  }

  scheduleRedraw(graphId) {
    this.pendingRedraws.add(graphId);
    if (!this.rafId) {
      this.rafId = requestAnimationFrame(() => {
        for (const id of this.pendingRedraws) {
          const graph = this.graphs.get(id);
          if (graph) graph.draw();
        }
        this.pendingRedraws.clear();
        this.rafId = null;
      });
    }
  }

  clearPlot(graphId, plotId) {
    const graph = this.graphs.get(graphId);
    if (!graph) return;
    graph.clearPlot(plotId);
    this.scheduleRedraw(graphId);
  }

  setPlotVisible(graphId, plotId, visible) {
    const graph = this.graphs.get(graphId);
    if (!graph) return;
    graph.setPlotVisible(plotId, visible);
    this.scheduleRedraw(graphId);
  }

  removeGraph(graphId) {
    const wrapper = document.getElementById(`graph-wrapper-${graphId}`);
    if (wrapper) wrapper.remove();
    this.graphs.delete(graphId);
    this.pendingRedraws.delete(graphId);
    if (this.graphs.size === 0) this.hidePanel();
  }

  setPaused(paused) {
    this.isPaused = paused;
    for (const graph of this.graphs.values()) {
      graph.setPaused(paused);
    }
  }

  resetViews() {
    for (const graph of this.graphs.values()) {
      graph.resetView();
    }
  }

  clear() {
    for (const [id] of this.graphs) {
      const wrapper = document.getElementById(`graph-wrapper-${id}`);
      if (wrapper) wrapper.remove();
    }
    this.graphs.clear();
    this.pendingRedraws.clear();
    if (this.rafId) {
      cancelAnimationFrame(this.rafId);
      this.rafId = null;
    }
    this.hidePanel();
  }

  setVisible(visible) {
    if (visible) this.showPanel();
    else this.hidePanel();
  }

  get count() {
    return this.graphs.size;
  }
}

window.vcppGraphManager = new VcppGraphManager();
console.log('[GraphManager] Module loaded');
