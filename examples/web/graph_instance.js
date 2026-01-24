/**
 * graph_instance.js - Canvas 2D rendering for a single graph
 *
 * Handles all 2D rendering for one graph including:
 * - Axes with tick marks and labels
 * - Grid lines
 * - Plot data (gcurve, gdots, gvbars)
 * - Legend
 * - Interactivity (hover, zoom/pan when paused)
 */

class GraphInstance {
  constructor(id, canvas, config) {
    this.id = id;
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');

    // Configuration
    this.width = config.width || 640;
    this.height = config.height || 400;
    this.background = config.background || [0.1, 0.1, 0.15];
    this.foreground = config.foreground || [0.9, 0.9, 0.9];
    this.title = config.title || '';
    this.xtitle = config.xtitle || '';
    this.ytitle = config.ytitle || '';

    // Axis bounds
    this.xmin = 0;
    this.xmax = 1;
    this.ymin = 0;
    this.ymax = 1;

    // Margins for axis labels
    this.margin = {
      top: this.title ? 40 : 20,
      right: 20,
      bottom: this.xtitle ? 50 : 35,
      left: this.ytitle ? 60 : 50
    };

    // Plot storage
    this.plots = new Map(); // plot_id -> plot data

    // Interaction state
    this.isPaused = false;
    this.hoverPoint = null;
    this.panStart = null;
    this.viewBounds = null; // Custom view bounds for zoom/pan

    // Set canvas size (accounting for device pixel ratio)
    this.setupCanvas();

    // Setup event listeners
    this.setupEvents();
  }

  setupCanvas() {
    const dpr = window.devicePixelRatio || 1;

    this.canvas.width = this.width * dpr;
    this.canvas.height = this.height * dpr;
    this.canvas.style.width = this.width + 'px';
    this.canvas.style.height = this.height + 'px';
    this.canvas.style.display = 'block';

    this.ctx = this.canvas.getContext('2d');
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    this.margin = {
      top: this.title ? 40 : 20,
      right: 20,
      bottom: this.xtitle ? 50 : 35,
      left: this.ytitle ? 60 : 50
    };
  }

  setupEvents() {
    this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
    this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
    this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
    this.canvas.addEventListener('wheel', (e) => this.onWheel(e));
    this.canvas.addEventListener('mouseleave', () => this.onMouseLeave());
  }

  // Convert data coordinates to canvas coordinates
  dataToCanvas(x, y) {
    const plotWidth = this.width - this.margin.left - this.margin.right;
    const plotHeight = this.height - this.margin.top - this.margin.bottom;

    const bounds = this.viewBounds || {
      xmin: this.xmin,
      xmax: this.xmax,
      ymin: this.ymin,
      ymax: this.ymax
    };

    const xRange = bounds.xmax - bounds.xmin || 1;
    const yRange = bounds.ymax - bounds.ymin || 1;

    const cx = this.margin.left + ((x - bounds.xmin) / xRange) * plotWidth;
    const cy = this.margin.top + plotHeight - ((y - bounds.ymin) / yRange) * plotHeight;

    return { x: cx, y: cy };
  }

  // Convert canvas coordinates to data coordinates
  canvasToData(cx, cy) {
    const plotWidth = this.width - this.margin.left - this.margin.right;
    const plotHeight = this.height - this.margin.top - this.margin.bottom;

    const bounds = this.viewBounds || {
      xmin: this.xmin,
      xmax: this.xmax,
      ymin: this.ymin,
      ymax: this.ymax
    };

    const xRange = bounds.xmax - bounds.xmin || 1;
    const yRange = bounds.ymax - bounds.ymin || 1;

    const x = bounds.xmin + ((cx - this.margin.left) / plotWidth) * xRange;
    const y = bounds.ymax - ((cy - this.margin.top) / plotHeight) * yRange;

    return { x, y };
  }

  // RGB array to CSS color string
  rgbToColor(rgb, alpha = 1) {
    return `rgba(${Math.round(rgb[0] * 255)}, ${Math.round(rgb[1] * 255)}, ${Math.round(rgb[2] * 255)}, ${alpha})`;
  }

  // Calculate nice tick values for an axis
  niceTickValues(min, max, targetTicks = 5) {
    const range = max - min;
    if (range <= 0) return [min];

    const roughStep = range / targetTicks;
    const magnitude = Math.pow(10, Math.floor(Math.log10(roughStep)));
    const residual = roughStep / magnitude;

    let niceStep;
    if (residual <= 1) niceStep = magnitude;
    else if (residual <= 2) niceStep = 2 * magnitude;
    else if (residual <= 5) niceStep = 5 * magnitude;
    else niceStep = 10 * magnitude;

    const ticks = [];
    let tick = Math.ceil(min / niceStep) * niceStep;
    while (tick <= max) {
      ticks.push(tick);
      tick += niceStep;
    }

    return ticks;
  }

  // Format tick value for display
  formatTick(value) {
    if (Math.abs(value) < 1e-10) return '0';
    if (Math.abs(value) >= 1000 || Math.abs(value) < 0.01) {
      return value.toExponential(1);
    }
    // Remove trailing zeros
    return parseFloat(value.toPrecision(4)).toString();
  }

  // Draw the graph
  draw() {
    const ctx = this.ctx;
    const w = this.width;
    const h = this.height;

    // Clear background
    ctx.fillStyle = this.rgbToColor(this.background);
    ctx.fillRect(0, 0, w, h);

    // Calculate plot area
    const plotLeft = this.margin.left;
    const plotTop = this.margin.top;
    const plotWidth = w - this.margin.left - this.margin.right;
    const plotHeight = h - this.margin.top - this.margin.bottom;

    // Get effective bounds
    const bounds = this.viewBounds || {
      xmin: this.xmin,
      xmax: this.xmax,
      ymin: this.ymin,
      ymax: this.ymax
    };

    // Draw grid
    ctx.strokeStyle = this.rgbToColor(this.foreground, 0.15);
    ctx.lineWidth = 1;

    const xTicks = this.niceTickValues(bounds.xmin, bounds.xmax);
    const yTicks = this.niceTickValues(bounds.ymin, bounds.ymax);

    ctx.beginPath();
    for (const x of xTicks) {
      const pos = this.dataToCanvas(x, 0);
      ctx.moveTo(pos.x, plotTop);
      ctx.lineTo(pos.x, plotTop + plotHeight);
    }
    for (const y of yTicks) {
      const pos = this.dataToCanvas(0, y);
      ctx.moveTo(plotLeft, pos.y);
      ctx.lineTo(plotLeft + plotWidth, pos.y);
    }
    ctx.stroke();

    // Draw axes
    ctx.strokeStyle = this.rgbToColor(this.foreground, 0.6);
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(plotLeft, plotTop);
    ctx.lineTo(plotLeft, plotTop + plotHeight);
    ctx.lineTo(plotLeft + plotWidth, plotTop + plotHeight);
    ctx.stroke();

    // Draw tick marks and labels
    ctx.fillStyle = this.rgbToColor(this.foreground);
    ctx.font = '11px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';

    for (const x of xTicks) {
      const pos = this.dataToCanvas(x, bounds.ymin);
      ctx.beginPath();
      ctx.moveTo(pos.x, plotTop + plotHeight);
      ctx.lineTo(pos.x, plotTop + plotHeight + 5);
      ctx.stroke();
      ctx.fillText(this.formatTick(x), pos.x, plotTop + plotHeight + 7);
    }

    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';

    for (const y of yTicks) {
      const pos = this.dataToCanvas(bounds.xmin, y);
      ctx.beginPath();
      ctx.moveTo(plotLeft - 5, pos.y);
      ctx.lineTo(plotLeft, pos.y);
      ctx.stroke();
      ctx.fillText(this.formatTick(y), plotLeft - 8, pos.y);
    }

    // Draw axis titles
    ctx.font = '12px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';

    if (this.xtitle) {
      ctx.fillText(this.xtitle, plotLeft + plotWidth / 2, h - 20);
    }

    if (this.ytitle) {
      ctx.save();
      ctx.translate(15, plotTop + plotHeight / 2);
      ctx.rotate(-Math.PI / 2);
      ctx.fillText(this.ytitle, 0, 0);
      ctx.restore();
    }

    // Draw title
    if (this.title) {
      ctx.font = 'bold 14px sans-serif';
      ctx.textBaseline = 'top';
      ctx.fillText(this.title, w / 2, 10);
    }

    // Clip to plot area for data rendering
    ctx.save();
    ctx.beginPath();
    ctx.rect(plotLeft, plotTop, plotWidth, plotHeight);
    ctx.clip();

    // Draw all plots
    for (const [plotId, plot] of this.plots) {
      if (!plot.visible || plot.data.length === 0) continue;

      switch (plot.type) {
        case 0: // gcurve
          this.drawGcurve(plot);
          break;
        case 1: // gdots
          this.drawGdots(plot);
          break;
        case 2: // gvbars
          this.drawGvbars(plot);
          break;
      }
    }

    ctx.restore();

    // Draw legend
    this.drawLegend();

    // Draw hover tooltip
    if (this.hoverPoint) {
      this.drawTooltip();
    }
  }

  drawGcurve(plot) {
    const ctx = this.ctx;

    if (plot.data.length < 2) {
      // Draw single point as dot
      if (plot.data.length === 1) {
        const pos = this.dataToCanvas(plot.data[0].x, plot.data[0].y);
        ctx.fillStyle = this.rgbToColor(plot.color);
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, 3, 0, Math.PI * 2);
        ctx.fill();
      }
      return;
    }

    ctx.strokeStyle = this.rgbToColor(plot.color);
    ctx.lineWidth = plot.width || 1;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';

    ctx.beginPath();
    const first = this.dataToCanvas(plot.data[0].x, plot.data[0].y);
    ctx.moveTo(first.x, first.y);

    for (let i = 1; i < plot.data.length; i++) {
      const pos = this.dataToCanvas(plot.data[i].x, plot.data[i].y);
      ctx.lineTo(pos.x, pos.y);
    }
    ctx.stroke();

    // Draw markers if enabled
    if (plot.markers) {
      ctx.fillStyle = this.rgbToColor(plot.color);
      for (const pt of plot.data) {
        const pos = this.dataToCanvas(pt.x, pt.y);
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, plot.markerRadius || 3, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }

  drawGdots(plot) {
    const ctx = this.ctx;
    ctx.fillStyle = this.rgbToColor(plot.color);

    for (const pt of plot.data) {
      const pos = this.dataToCanvas(pt.x, pt.y);
      ctx.beginPath();
      ctx.arc(pos.x, pos.y, plot.radius || 3, 0, Math.PI * 2);
      ctx.fill();
    }
  }

  drawGvbars(plot) {
    const ctx = this.ctx;
    ctx.fillStyle = this.rgbToColor(plot.color, 0.8);
    ctx.strokeStyle = this.rgbToColor(plot.color);
    ctx.lineWidth = 1;

    const barWidth = plot.delta || 1;
    const halfWidth = barWidth / 2;

    for (const pt of plot.data) {
      const topLeft = this.dataToCanvas(pt.x - halfWidth, pt.y);
      const bottomRight = this.dataToCanvas(pt.x + halfWidth, 0);

      const x = topLeft.x;
      const y = Math.min(topLeft.y, bottomRight.y);
      const w = bottomRight.x - topLeft.x;
      const h = Math.abs(bottomRight.y - topLeft.y);

      ctx.fillRect(x, y, w, h);
      ctx.strokeRect(x, y, w, h);
    }
  }

  drawLegend() {
    const ctx = this.ctx;
    const legendItems = [];

    for (const [plotId, plot] of this.plots) {
      if (plot.legend && plot.label) {
        legendItems.push({
          label: plot.label,
          color: plot.color,
          type: plot.type
        });
      }
    }

    if (legendItems.length === 0) return;

    const padding = 8;
    const itemHeight = 18;
    const legendWidth = 120;
    const legendHeight = padding * 2 + legendItems.length * itemHeight;

    const x = this.width - this.margin.right - legendWidth - 10;
    const y = this.margin.top + 10;

    // Background
    ctx.fillStyle = this.rgbToColor(this.background, 0.85);
    ctx.strokeStyle = this.rgbToColor(this.foreground, 0.3);
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(x, y, legendWidth, legendHeight, 4);
    ctx.fill();
    ctx.stroke();

    // Items
    ctx.font = '11px sans-serif';
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';

    legendItems.forEach((item, i) => {
      const itemY = y + padding + i * itemHeight + itemHeight / 2;

      // Color swatch
      ctx.fillStyle = this.rgbToColor(item.color);
      if (item.type === 0) {
        // Line for gcurve
        ctx.beginPath();
        ctx.moveTo(x + padding, itemY);
        ctx.lineTo(x + padding + 20, itemY);
        ctx.strokeStyle = this.rgbToColor(item.color);
        ctx.lineWidth = 2;
        ctx.stroke();
      } else if (item.type === 1) {
        // Dot for gdots
        ctx.beginPath();
        ctx.arc(x + padding + 10, itemY, 4, 0, Math.PI * 2);
        ctx.fill();
      } else {
        // Rectangle for gvbars
        ctx.fillRect(x + padding, itemY - 6, 20, 12);
      }

      // Label
      ctx.fillStyle = this.rgbToColor(this.foreground);
      ctx.fillText(item.label, x + padding + 28, itemY);
    });
  }

  drawTooltip() {
    if (!this.hoverPoint) return;

    const ctx = this.ctx;
    const { x, y, dataX, dataY } = this.hoverPoint;

    const text = `(${this.formatTick(dataX)}, ${this.formatTick(dataY)})`;
    ctx.font = '11px sans-serif';
    const textWidth = ctx.measureText(text).width;

    const padding = 6;
    const tooltipWidth = textWidth + padding * 2;
    const tooltipHeight = 20;

    let tx = x + 10;
    let ty = y - tooltipHeight - 5;

    // Keep tooltip in bounds
    if (tx + tooltipWidth > this.width) tx = x - tooltipWidth - 10;
    if (ty < 0) ty = y + 10;

    ctx.fillStyle = this.rgbToColor(this.background, 0.9);
    ctx.strokeStyle = this.rgbToColor(this.foreground, 0.5);
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.roundRect(tx, ty, tooltipWidth, tooltipHeight, 3);
    ctx.fill();
    ctx.stroke();

    ctx.fillStyle = this.rgbToColor(this.foreground);
    ctx.textAlign = 'left';
    ctx.textBaseline = 'middle';
    ctx.fillText(text, tx + padding, ty + tooltipHeight / 2);

    // Draw highlight point
    ctx.beginPath();
    ctx.arc(x, y, 5, 0, Math.PI * 2);
    ctx.fillStyle = this.rgbToColor(this.foreground, 0.3);
    ctx.fill();
    ctx.strokeStyle = this.rgbToColor(this.foreground);
    ctx.lineWidth = 2;
    ctx.stroke();
  }

  // Event handlers
  onMouseMove(e) {
    const rect = this.canvas.getBoundingClientRect();
    const cx = e.clientX - rect.left;
    const cy = e.clientY - rect.top;

    // Check if in plot area
    const inPlot = cx >= this.margin.left &&
                   cx <= this.width - this.margin.right &&
                   cy >= this.margin.top &&
                   cy <= this.height - this.margin.bottom;

    if (!inPlot) {
      if (this.hoverPoint) {
        this.hoverPoint = null;
        this.draw();
      }
      return;
    }

    // Pan while dragging (when paused)
    if (this.isPaused && this.panStart) {
      const dx = cx - this.panStart.x;
      const dy = cy - this.panStart.y;

      const plotWidth = this.width - this.margin.left - this.margin.right;
      const plotHeight = this.height - this.margin.top - this.margin.bottom;

      const bounds = this.viewBounds || {
        xmin: this.xmin,
        xmax: this.xmax,
        ymin: this.ymin,
        ymax: this.ymax
      };

      const xRange = bounds.xmax - bounds.xmin;
      const yRange = bounds.ymax - bounds.ymin;

      const dxData = -dx * xRange / plotWidth;
      const dyData = dy * yRange / plotHeight;

      this.viewBounds = {
        xmin: this.panStart.bounds.xmin + dxData,
        xmax: this.panStart.bounds.xmax + dxData,
        ymin: this.panStart.bounds.ymin + dyData,
        ymax: this.panStart.bounds.ymax + dyData
      };

      this.panStart.x = cx;
      this.panStart.y = cy;
      this.panStart.bounds = { ...this.viewBounds };

      this.draw();
      return;
    }

    // Find nearest point for hover
    const data = this.canvasToData(cx, cy);
    let nearest = null;
    let minDist = Infinity;

    for (const [plotId, plot] of this.plots) {
      if (!plot.visible) continue;

      for (const pt of plot.data) {
        const pos = this.dataToCanvas(pt.x, pt.y);
        const dist = Math.sqrt((pos.x - cx) ** 2 + (pos.y - cy) ** 2);
        if (dist < minDist && dist < 20) {
          minDist = dist;
          nearest = { x: pos.x, y: pos.y, dataX: pt.x, dataY: pt.y };
        }
      }
    }

    if (nearest !== this.hoverPoint) {
      this.hoverPoint = nearest;
      this.draw();
    }
  }

  onMouseDown(e) {
    if (!this.isPaused) return;

    const rect = this.canvas.getBoundingClientRect();
    const cx = e.clientX - rect.left;
    const cy = e.clientY - rect.top;

    const bounds = this.viewBounds || {
      xmin: this.xmin,
      xmax: this.xmax,
      ymin: this.ymin,
      ymax: this.ymax
    };

    this.panStart = { x: cx, y: cy, bounds: { ...bounds } };
    this.canvas.style.cursor = 'grabbing';
  }

  onMouseUp(e) {
    this.panStart = null;
    this.canvas.style.cursor = this.isPaused ? 'grab' : 'default';
  }

  onMouseLeave() {
    this.hoverPoint = null;
    this.panStart = null;
    this.canvas.style.cursor = 'default';
    this.draw();
  }

  onWheel(e) {
    if (!this.isPaused) return;
    e.preventDefault();

    const rect = this.canvas.getBoundingClientRect();
    const cx = e.clientX - rect.left;
    const cy = e.clientY - rect.top;

    // Check if in plot area
    const inPlot = cx >= this.margin.left &&
                   cx <= this.width - this.margin.right &&
                   cy >= this.margin.top &&
                   cy <= this.height - this.margin.bottom;

    if (!inPlot) return;

    const zoomFactor = e.deltaY > 0 ? 1.1 : 0.9;
    const data = this.canvasToData(cx, cy);

    const bounds = this.viewBounds || {
      xmin: this.xmin,
      xmax: this.xmax,
      ymin: this.ymin,
      ymax: this.ymax
    };

    // Zoom centered on mouse position
    this.viewBounds = {
      xmin: data.x - (data.x - bounds.xmin) * zoomFactor,
      xmax: data.x + (bounds.xmax - data.x) * zoomFactor,
      ymin: data.y - (data.y - bounds.ymin) * zoomFactor,
      ymax: data.y + (bounds.ymax - data.y) * zoomFactor
    };

    this.draw();
  }

  // API methods
  updateBounds(xmin, xmax, ymin, ymax) {
    this.xmin = xmin;
    this.xmax = xmax;
    this.ymin = ymin;
    this.ymax = ymax;

    // Reset view bounds if not paused
    if (!this.isPaused) {
      this.viewBounds = null;
    }
  }

  createPlot(plotId, type, color, label, widthOrRadius, delta) {
    this.plots.set(plotId, {
      type,
      color,
      label,
      width: type === 0 ? widthOrRadius : 1,
      radius: type === 1 ? widthOrRadius : 3,
      delta: type === 2 ? delta : 1,
      markers: false,
      markerRadius: 3,
      visible: true,
      legend: !!label,
      data: []
    });
  }

  addPoint(plotId, x, y) {
    const plot = this.plots.get(plotId);
    if (plot) {
      plot.data.push({ x, y });
    }
  }

  addPointsBatch(plotId, points) {
    const plot = this.plots.get(plotId);
    if (plot) {
      plot.data.push(...points);
    }
  }

  clearPlot(plotId) {
    const plot = this.plots.get(plotId);
    if (plot) {
      plot.data = [];
    }
  }

  setPlotVisible(plotId, visible) {
    const plot = this.plots.get(plotId);
    if (plot) {
      plot.visible = visible;
    }
  }

  setPaused(paused) {
    this.isPaused = paused;
    this.canvas.style.cursor = paused ? 'grab' : 'default';

    if (!paused) {
      this.viewBounds = null; // Reset view on resume
    }
  }

  resetView() {
    this.viewBounds = null;
    this.draw();
  }
}

// Export for use in graph_manager.js
window.GraphInstance = GraphInstance;
