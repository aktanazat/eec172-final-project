const pptxgen = require("/private/tmp/pptx-tools/node_modules/pptxgenjs");
const path = require("path");

const pptx = new pptxgen();
pptx.layout = "LAYOUT_WIDE";
pptx.author = "OpenAI Codex";
pptx.company = "EEC172";
pptx.subject = "AEGIS-172 progress presentation";
pptx.title = "AEGIS-172 In-class Presentation";
pptx.lang = "en-US";

const COLORS = {
  bg: "081421",
  bg2: "0E2235",
  panel: "11293F",
  panel2: "163650",
  ink: "F4F7FB",
  muted: "A9BDD3",
  cyan: "42D9FF",
  amber: "FFB44C",
  red: "FF6B6B",
  green: "69E5A6",
  yellow: "FFE37A",
  line: "274862",
  white: "FFFFFF",
  dark: "09111A"
};

const OWNER = {
  aktan: COLORS.amber,
  gezheng: COLORS.cyan,
  both: COLORS.green
};

function addBackdrop(slide) {
  slide.background = { color: COLORS.bg };
  slide.addShape(pptx.ShapeType.rect, {
    x: 0,
    y: 0,
    w: 13.333,
    h: 7.5,
    fill: { color: COLORS.bg },
    line: { color: COLORS.bg }
  });
  slide.addShape(pptx.ShapeType.rect, {
    x: 0,
    y: 0,
    w: 13.333,
    h: 0.55,
    fill: { color: COLORS.bg2 },
    line: { color: COLORS.bg2 }
  });
  slide.addShape(pptx.ShapeType.line, {
    x: 0.55,
    y: 0.82,
    w: 12.1,
    h: 0,
    line: { color: COLORS.line, width: 1.25 }
  });
}

function addRadarMotif(slide, x, y, size, color, alpha) {
  slide.addShape(pptx.ShapeType.arc, {
    x,
    y,
    w: size,
    h: size,
    fill: { color: COLORS.bg, transparency: 100 },
    line: { color, transparency: alpha, width: 1.2 }
  });
  slide.addShape(pptx.ShapeType.arc, {
    x: x + size * 0.15,
    y: y + size * 0.15,
    w: size * 0.7,
    h: size * 0.7,
    fill: { color: COLORS.bg, transparency: 100 },
    line: { color, transparency: alpha + 10, width: 1.2 }
  });
  slide.addShape(pptx.ShapeType.arc, {
    x: x + size * 0.3,
    y: y + size * 0.3,
    w: size * 0.4,
    h: size * 0.4,
    fill: { color: COLORS.bg, transparency: 100 },
    line: { color, transparency: alpha + 15, width: 1.2 }
  });
  slide.addShape(pptx.ShapeType.line, {
    x: x + size * 0.5,
    y: y + size * 0.5,
    w: size * 0.35,
    h: -size * 0.28,
    line: { color, transparency: alpha - 20, width: 2 }
  });
}

function addTitle(slide, kicker, title, subtitle, opts = {}) {
  const titleY = opts.titleY || 0.95;
  const titleH = opts.titleH || 0.52;
  const titleSize = opts.titleSize || 26;
  const titleW = opts.titleW || 8.9;
  const subtitleY = opts.subtitleY || 1.42;
  const subtitleH = opts.subtitleH || 0.35;
  const subtitleSize = opts.subtitleSize || 12.5;
  slide.addText(kicker, {
    x: 0.58,
    y: 0.22,
    w: 3.5,
    h: 0.2,
    fontFace: "Trebuchet MS",
    fontSize: 10,
    bold: true,
    color: COLORS.cyan,
    charSpacing: 1.6,
    margin: 0
  });
  slide.addText(title, {
    x: 0.58,
    y: titleY,
    w: titleW,
    h: titleH,
    fontFace: "Trebuchet MS",
    fontSize: titleSize,
    bold: true,
    color: COLORS.ink,
    margin: 0
  });
  slide.addText(subtitle, {
    x: 0.58,
    y: subtitleY,
    w: 8.8,
    h: subtitleH,
    fontFace: "Calibri",
    fontSize: subtitleSize,
    color: COLORS.muted,
    margin: 0
  });
}

function addFooter(slide, rightText) {
  slide.addText("AEGIS-172 | EEC172 Final Project", {
    x: 0.58,
    y: 7.08,
    w: 4.1,
    h: 0.15,
    fontFace: "Calibri",
    fontSize: 9,
    color: COLORS.muted,
    margin: 0
  });
  slide.addText(rightText, {
    x: 9.2,
    y: 7.08,
    w: 3.55,
    h: 0.15,
    fontFace: "Calibri",
    fontSize: 9,
    color: COLORS.muted,
    align: "right",
    margin: 0
  });
}

function addPill(slide, text, x, y, w, fill, textColor = COLORS.dark) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x,
    y,
    w,
    h: 0.28,
    rectRadius: 0.06,
    fill: { color: fill },
    line: { color: fill }
  });
  slide.addText(text, {
    x,
    y: y + 0.03,
    w,
    h: 0.18,
    fontFace: "Calibri",
    fontSize: 10,
    bold: true,
    color: textColor,
    align: "center",
    margin: 0
  });
}

function addPanel(slide, x, y, w, h, title, bodyLines, opts = {}) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x,
    y,
    w,
    h,
    rectRadius: 0.08,
    fill: { color: opts.fill || COLORS.panel },
    line: { color: opts.line || COLORS.line, width: opts.lineWidth || 1.2 },
    shadow: { type: "outer", color: "000000", blur: 2, offset: 1, angle: 45, opacity: 0.18 }
  });

  slide.addText(title, {
    x: x + 0.16,
    y: y + 0.12,
    w: w - 0.32,
    h: 0.25,
    fontFace: "Trebuchet MS",
    fontSize: 14,
    bold: true,
    color: COLORS.ink,
    margin: 0
  });

  const bullets = [];
  bodyLines.forEach((line, idx) => {
    bullets.push({
      text: line,
      options: { bullet: true, breakLine: idx !== bodyLines.length - 1 }
    });
  });

  slide.addText(bullets, {
    x: x + 0.18,
    y: y + 0.47,
    w: w - 0.36,
    h: h - 0.58,
    fontFace: "Calibri",
    fontSize: opts.fontSize || 11.5,
    color: COLORS.ink,
    breakLine: true,
    paraSpaceAfterPt: 6,
    margin: 0.02,
    valign: "top"
  });
}

function addMetricCard(slide, x, y, w, h, value, label, accent) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x,
    y,
    w,
    h,
    rectRadius: 0.07,
    fill: { color: COLORS.panel2 },
    line: { color: accent, width: 1.5 }
  });
  slide.addText(value, {
    x: x + 0.15,
    y: y + 0.12,
    w: w - 0.3,
    h: 0.38,
    fontFace: "Trebuchet MS",
    fontSize: 22,
    bold: true,
    color: accent,
    margin: 0
  });
  slide.addText(label, {
    x: x + 0.15,
    y: y + 0.53,
    w: w - 0.3,
    h: 0.22,
    fontFace: "Calibri",
    fontSize: 10.5,
    color: COLORS.muted,
    margin: 0
  });
}

function addOwnerLegend(slide, x, y) {
  addPill(slide, "Aktan lead", x, y, 1.1, OWNER.aktan);
  addPill(slide, "Gezheng lead", x + 1.22, y, 1.2, OWNER.gezheng);
  addPill(slide, "Shared", x + 2.56, y, 0.82, OWNER.both);
}

function addArrow(slide, x1, y1, x2, y2, label, color = COLORS.cyan) {
  slide.addShape(pptx.ShapeType.line, {
    x: x1,
    y: y1,
    w: x2 - x1,
    h: y2 - y1,
    line: { color, width: 1.5, endArrowType: "triangle" }
  });
  if (label) {
    slide.addText(label, {
      x: Math.min(x1, x2) + Math.abs(x2 - x1) / 2 - 0.45,
      y: Math.min(y1, y2) + Math.abs(y2 - y1) / 2 - 0.12,
      w: 0.9,
      h: 0.18,
      fontFace: "Calibri",
      fontSize: 9.5,
      color: color,
      bold: true,
      align: "center",
      fill: { color: COLORS.bg },
      margin: 0
    });
  }
}

function addOwnedCard(slide, x, y, w, h, ownerColor, title, lines, statusText, statusColor) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x,
    y,
    w,
    h,
    rectRadius: 0.06,
    fill: { color: COLORS.panel },
    line: { color: ownerColor, width: 2 }
  });
  addPill(slide, statusText, x + w - 0.86, y + 0.12, 0.72, statusColor, COLORS.dark);
  slide.addText(title, {
    x: x + 0.15,
    y: y + 0.14,
    w: w - 1.05,
    h: 0.2,
    fontFace: "Trebuchet MS",
    fontSize: 13,
    bold: true,
    color: COLORS.ink,
    margin: 0
  });
  const body = lines
    .map((line, idx) => ({ text: line, options: { breakLine: idx !== lines.length - 1 } }));
  slide.addText(body, {
    x: x + 0.16,
    y: y + 0.48,
    w: w - 0.3,
    h: h - 0.58,
    fontFace: "Calibri",
    fontSize: 10.4,
    color: COLORS.ink,
    paraSpaceAfterPt: 7,
    margin: 0.02
  });
}

function addStatusRow(slide, x, y, w, label, items, fill, border) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x,
    y,
    w,
    h: 1.15,
    rectRadius: 0.05,
    fill: { color: fill },
    line: { color: border, width: 1.5 }
  });
  slide.addText(label, {
    x: x + 0.15,
    y: y + 0.14,
    w: 1.7,
    h: 0.2,
    fontFace: "Trebuchet MS",
    fontSize: 14,
    bold: true,
    color: COLORS.ink,
    margin: 0
  });
  const bullets = [];
  items.forEach((line, idx) => {
    bullets.push({ text: line, options: { bullet: true, breakLine: idx !== items.length - 1 } });
  });
  slide.addText(bullets, {
    x: x + 1.95,
    y: y + 0.14,
    w: w - 2.1,
    h: 0.82,
    fontFace: "Calibri",
    fontSize: 11,
    color: COLORS.ink,
    paraSpaceAfterPt: 5,
    margin: 0.02
  });
}

function addTimelineChip(slide, x, y, w, title, body, fill) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x,
    y,
    w,
    h: 0.78,
    rectRadius: 0.05,
    fill: { color: fill },
    line: { color: fill }
  });
  slide.addText(title, {
    x: x + 0.12,
    y: y + 0.1,
    w: w - 0.24,
    h: 0.2,
    fontFace: "Trebuchet MS",
    fontSize: 12.5,
    bold: true,
    color: COLORS.dark,
    margin: 0
  });
  slide.addText(body, {
    x: x + 0.12,
    y: y + 0.34,
    w: w - 0.24,
    h: 0.24,
    fontFace: "Calibri",
    fontSize: 9.5,
    color: COLORS.dark,
    margin: 0
  });
}

function addPlayFrame(slide, x, y, w, h) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x,
    y,
    w,
    h,
    rectRadius: 0.08,
    fill: { color: COLORS.panel2 },
    line: { color: COLORS.cyan, width: 1.4, dash: "dash" }
  });
  slide.addShape(pptx.ShapeType.triangle, {
    x: x + w / 2 - 0.3,
    y: y + h / 2 - 0.36,
    w: 0.65,
    h: 0.72,
    rotate: 90,
    fill: { color: COLORS.cyan },
    line: { color: COLORS.cyan }
  });
  slide.addText("Insert 10-15s partial demo clip here", {
    x: x + 0.55,
    y: y + h - 0.78,
    w: w - 1.1,
    h: 0.22,
    fontFace: "Trebuchet MS",
    fontSize: 16,
    bold: true,
    color: COLORS.ink,
    align: "center",
    margin: 0
  });
  slide.addText("Suggested sequence: boot -> sweep -> shield move -> score update -> cloud request overlay", {
    x: x + 0.45,
    y: y + h - 0.47,
    w: w - 0.9,
    h: 0.18,
    fontFace: "Calibri",
    fontSize: 10.5,
    color: COLORS.muted,
    align: "center",
    margin: 0
  });
}

function slideObjectives() {
  const slide = pptx.addSlide();
  addBackdrop(slide);
  addRadarMotif(slide, 8.85, 0.95, 3.55, COLORS.cyan, 70);
  addRadarMotif(slide, 9.85, 3.95, 2.2, COLORS.amber, 78);
  addTitle(
    slide,
    "PROJECT OBJECTIVES",
    "AEGIS-172: cloud-adaptive two-player radar defense arena",
    "A standalone embedded game system that combines physical scanning, asymmetric controls, and AWS-driven missions."
  );

  slide.addText([
    { text: "What we are building", options: { bold: true, breakLine: true } },
    { text: "CC3200-led tabletop arena with OLED HUD, real-time radar sweep, servo shield control, and cloud-scored competitive rounds.", options: { breakLine: true } },
    { text: "Why it stands out", options: { bold: true, breakLine: true } },
    { text: "Instead of a lock, tuner, or simple arcade clone, the system blends mechatronics, two-player interaction, and AWS mission generation in one standalone device." }
  ], {
    x: 0.7,
    y: 1.95,
    w: 6.45,
    h: 2.22,
    fontFace: "Calibri",
    fontSize: 14,
    color: COLORS.ink,
    margin: 0.02,
    breakLine: true,
    paraSpaceAfterPt: 8
  });

  addMetricCard(slide, 0.72, 4.55, 1.75, 0.95, "3", "AWS services in critical path", COLORS.cyan);
  addMetricCard(slide, 2.62, 4.55, 1.75, 0.95, "5+", "sensors fused into threat score", COLORS.amber);
  addMetricCard(slide, 4.52, 4.55, 1.75, 0.95, "2", "MCUs split by timing domain", COLORS.green);

  addPanel(slide, 7.5, 1.85, 5.0, 2.75, "Comparable products and our difference", [
    "Closest references: radar toys, reaction games, and simple home security prototypes",
    "Our pivot adds competitive two-player roles instead of passive monitoring",
    "Mission scripts come from AWS IoT + Lambda + S3 instead of fixed local levels",
    "Replay artifacts and leaderboard data make the demo measurable and reproducible"
  ], { fill: COLORS.panel2, line: COLORS.line, fontSize: 11 });

  addPanel(slide, 7.5, 4.9, 5.0, 1.55, "Presentation plan", [
    "1 slide objective, 2 slides architecture/tasks, 2 slides progress + demo evidence"
  ], { fill: COLORS.panel, line: COLORS.line, fontSize: 11 });

  addFooter(slide, "Slide 1 | Objectives");
}

function slideArchitecture() {
  const slide = pptx.addSlide();
  addBackdrop(slide);
  addTitle(
    slide,
    "SYSTEM MODULES",
    "Block diagram: data flow, control flow, and cloud path",
    "The project splits deterministic hardware control onto the UNO while the CC3200 owns UI, state, and cloud traffic."
  );
  addOwnerLegend(slide, 9.05, 0.24);

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 5.05,
    y: 2.0,
    w: 3.1,
    h: 2.2,
    rectRadius: 0.08,
    fill: { color: COLORS.panel2 },
    line: { color: OWNER.aktan, width: 2 }
  });
  slide.addText("CC3200 Main Controller", {
    x: 5.25,
    y: 2.18,
    w: 2.7,
    h: 0.24,
    fontFace: "Trebuchet MS",
    fontSize: 16,
    bold: true,
    color: COLORS.ink,
    align: "center",
    margin: 0
  });
  slide.addText([
    { text: "match state machine", options: { breakLine: true } },
    { text: "OLED HUD + scoreboard", options: { breakLine: true } },
    { text: "AWS IoT / Lambda / S3 client", options: { breakLine: true } },
    { text: "UART control exchange", options: {} }
  ], {
    x: 5.5,
    y: 2.55,
    w: 2.2,
    h: 1.25,
    fontFace: "Calibri",
    fontSize: 12,
    color: COLORS.ink,
    align: "center",
    margin: 0
  });
  addPill(slide, "Aktan lead", 6.02, 3.78, 1.18, OWNER.aktan);

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 5.05,
    y: 4.72,
    w: 3.1,
    h: 1.75,
    rectRadius: 0.08,
    fill: { color: COLORS.panel },
    line: { color: OWNER.gezheng, width: 2 }
  });
  slide.addText("UNO Coprocessor", {
    x: 5.35,
    y: 4.92,
    w: 2.5,
    h: 0.24,
    fontFace: "Trebuchet MS",
    fontSize: 15,
    bold: true,
    color: COLORS.ink,
    align: "center",
    margin: 0
  });
  slide.addText([
    { text: "stepper sweep + ultrasonic sectors", options: { breakLine: true } },
    { text: "servo, RGB, buzzer outputs", options: { breakLine: true } },
    { text: "sensor frames back to CC3200", options: {} }
  ], {
    x: 5.35,
    y: 5.27,
    w: 2.52,
    h: 0.8,
    fontFace: "Calibri",
    fontSize: 11.5,
    color: COLORS.ink,
    align: "center",
    margin: 0
  });
  addPill(slide, "Gezheng lead", 5.93, 6.02, 1.34, OWNER.gezheng);

  addPanel(slide, 0.72, 2.0, 2.4, 1.35, "Player Inputs", [
    "Joystick: defender shield placement",
    "Start button: local round control"
  ], { line: OWNER.both, fill: COLORS.panel });
  addPanel(slide, 0.72, 4.08, 2.4, 1.35, "Attacker Channel", [
    "IR remote injects actions",
    "Asymmetric two-player interaction"
  ], { line: OWNER.both, fill: COLORS.panel });
  addPanel(slide, 9.42, 1.92, 2.9, 1.55, "AWS Path", [
    "IoT: mission request + telemetry",
    "Lambda: scoring + mission generation",
    "S3: replay and leaderboard artifacts"
  ], { line: OWNER.aktan, fill: COLORS.panel2, fontSize: 10.8 });
  addPanel(slide, 9.42, 4.18, 2.9, 1.95, "Physical Arena", [
    "stepper + ULN2003 radar head",
    "ultrasonic, tilt, light, DHT11",
    "servo shield, buzzer, RGB"
  ], { line: OWNER.gezheng, fill: COLORS.panel, fontSize: 10.8 });

  addArrow(slide, 3.12, 2.7, 5.05, 2.7, "analog");
  addArrow(slide, 3.12, 4.75, 5.05, 3.2, "IR");
  addArrow(slide, 8.15, 2.85, 9.42, 2.85, "TLS");
  addArrow(slide, 6.6, 4.18, 6.6, 4.72, "UART");
  addArrow(slide, 8.15, 5.52, 9.42, 5.12, "control");
  addArrow(slide, 9.42, 3.2, 8.15, 3.2, "missions", COLORS.amber);

  addFooter(slide, "Slide 2 | Architecture");
}

function slideTasks() {
  const slide = pptx.addSlide();
  addBackdrop(slide);
  addTitle(
    slide,
    "TASK MODULES",
    "Ownership map and dependency flow",
    "Color coding matches the team member leading each task, with shared integration work shown separately."
  );
  addOwnerLegend(slide, 9.05, 0.24);

  slide.addText("Aktan", {
    x: 0.8,
    y: 1.65,
    w: 3.2,
    h: 0.25,
    fontFace: "Trebuchet MS",
    fontSize: 17,
    bold: true,
    color: OWNER.aktan,
    margin: 0
  });
  slide.addText("Shared integration", {
    x: 4.85,
    y: 1.65,
    w: 3.2,
    h: 0.25,
    fontFace: "Trebuchet MS",
    fontSize: 17,
    bold: true,
    color: OWNER.both,
    align: "center",
    margin: 0
  });
  slide.addText("Gezheng", {
    x: 9.28,
    y: 1.65,
    w: 3.0,
    h: 0.25,
    fontFace: "Trebuchet MS",
    fontSize: 17,
    bold: true,
    color: OWNER.gezheng,
    align: "right",
    margin: 0
  });

  addOwnedCard(slide, 0.72, 2.0, 3.25, 1.32, OWNER.aktan, "CC3200 game loop", [
    "replace ParkPilot modes with round states",
    "OLED radar HUD and scoring view"
  ], "Now", COLORS.yellow);
  addOwnedCard(slide, 0.72, 3.62, 3.25, 1.32, OWNER.aktan, "AWS pipeline", [
    "IoT shadow schema",
    "Lambda mission + score handlers"
  ], "Next", COLORS.amber);
  addOwnedCard(slide, 0.72, 5.24, 3.25, 1.32, OWNER.aktan, "Presentation + website", [
    "deck, report, webpage polish",
    "final evidence packaging"
  ], "Done", COLORS.green);

  addOwnedCard(slide, 5.05, 2.0, 3.22, 1.32, OWNER.both, "UART frame contract", [
    "half-duplex control/sensor exchange",
    "reuse same protocol for CC3200 swap"
  ], "Done", COLORS.green);
  addOwnedCard(slide, 5.05, 3.62, 3.22, 1.32, OWNER.both, "System integration", [
    "power stability, level shifting, task timing",
    "full round validation"
  ], "Now", COLORS.yellow);
  addOwnedCard(slide, 5.05, 5.24, 3.22, 1.32, OWNER.both, "Demo choreography", [
    "4-minute script, clean wiring, recovery path",
    "video capture and judge-facing proof"
  ], "Planned", COLORS.red);

  addOwnedCard(slide, 9.35, 2.0, 3.25, 1.32, OWNER.gezheng, "UNO mechatronics", [
    "stepper sweep, servo shield, buzzer, RGB",
    "joystick and IR local handling"
  ], "Now", COLORS.yellow);
  addOwnedCard(slide, 9.35, 3.62, 3.25, 1.32, OWNER.gezheng, "Sensor fusion inputs", [
    "ultrasonic, tilt, light, DHT11",
    "noise-aware sampling"
  ], "Now", COLORS.yellow);
  addOwnedCard(slide, 9.35, 5.24, 3.25, 1.32, OWNER.gezheng, "Reliability pass", [
    "stable 5V rail and wiring cleanup",
    "repeatable live demonstration behavior"
  ], "Next", COLORS.amber);

  addArrow(slide, 3.97, 2.66, 5.05, 2.66, "");
  addArrow(slide, 3.97, 4.28, 5.05, 4.28, "");
  addArrow(slide, 8.27, 2.66, 9.35, 2.66, "");
  addArrow(slide, 8.27, 4.28, 9.35, 4.28, "");
  addArrow(slide, 6.66, 3.32, 6.66, 3.62, "");
  addArrow(slide, 6.66, 4.94, 6.66, 5.24, "");

  addFooter(slide, "Slide 3 | Tasks + Ownership");
}

function slideProgress() {
  const slide = pptx.addSlide();
  addBackdrop(slide);
  addTitle(
    slide,
    "PROGRESS REPORT",
    "Current status, remaining work, and finish timeline",
    "The goal is to enter lecture with a local playable build first, then lock cloud and polish second."
  );

  addStatusRow(slide, 0.72, 1.95, 11.9, "Completed", [
    "Project pivot finalized: AEGIS-172 concept, site rewrite, proposal, and architecture",
    "Dual-UNO prototype path established with working request/response UART frame contract",
    "Local state machine, joystick shield control, stepper sweep, and actuator loop verified"
  ], COLORS.panel2, COLORS.green);

  addStatusRow(slide, 0.72, 3.35, 11.9, "In progress", [
    "CC3200 master port from existing ParkPilot firmware and OLED HUD redesign",
    "UNO sensor wiring cleanup and final pin map for joystick, IR, ultrasonic, tilt, light, DHT11",
    "Scoring balance so threat level, buzzer, and RGB are readable in demo conditions"
  ], COLORS.panel, COLORS.yellow);

  addStatusRow(slide, 0.72, 4.75, 11.9, "Planned next", [
    "AWS IoT mission request, Lambda scoring, and S3 replay artifact integration",
    "Demo video capture, enclosure cleanup, and fallback local mission mode",
    "Final lecture script and live verification checklist"
  ], COLORS.panel, COLORS.amber);

  addTimelineChip(slide, 0.88, 6.35, 2.6, "Tonight", "stabilize local game loop and presentation", COLORS.cyan);
  addTimelineChip(slide, 3.62, 6.35, 2.85, "Tomorrow", "swap in CC3200 and light up OLED HUD", COLORS.amber);
  addTimelineChip(slide, 6.62, 6.35, 2.85, "Next 48h", "wire AWS mission + replay path", COLORS.green);
  addTimelineChip(slide, 9.62, 6.35, 2.85, "Before lecture", "record clip and rehearse 4-minute run", COLORS.yellow);

  addFooter(slide, "Slide 4 | Progress");
}

function slideDemo() {
  const slide = pptx.addSlide();
  addBackdrop(slide);
  addTitle(
    slide,
    "PARTIAL DEMO",
    "Demo slot plus speaking cues for the lecture presentation",
    "This slide is structured for the required 10-15 second clip and a quick verbal walkthrough of what is already working.",
    { titleSize: 22, titleH: 0.72, subtitleY: 1.6, subtitleH: 0.24, subtitleSize: 11.5 }
  );

  addPlayFrame(slide, 0.72, 1.85, 7.65, 4.7);

  addPanel(slide, 8.75, 1.85, 3.85, 2.2, "Current verified pieces", [
    "master-coprocessor UART loop",
    "state transitions and score ticks",
    "stepper radar sweep + joystick shield motion",
    "presentation website and architecture"
  ], { fill: COLORS.panel2, line: COLORS.green, fontSize: 11 });

  addPanel(slide, 8.75, 4.25, 3.85, 2.3, "What to narrate over the clip", [
    "boot from flash with no host control",
    "defender shield tracks joystick sector",
    "ultrasonic sweep drives threat pressure",
    "same frame contract will move to CC3200 + AWS"
  ], { fill: COLORS.panel, line: COLORS.cyan, fontSize: 11 });

  slide.addText("Before class: replace the placeholder with a real MP4 clip from the prototype. The rest of the slide is ready.", {
    x: 0.85,
    y: 6.78,
    w: 7.45,
    h: 0.2,
    fontFace: "Calibri",
    fontSize: 10.5,
    color: COLORS.muted,
    margin: 0
  });

  addFooter(slide, "Slide 5 | Demo Placeholder");
}

slideObjectives();
slideArchitecture();
slideTasks();
slideProgress();
slideDemo();

const out = path.join(
  "/Users/aktanazat/Documents/coursework/SQ25/EEC172/CCS/final-project/presentation",
  "AEGIS-172-in-class-presentation.pptx"
);

pptx.writeFile({ fileName: out });
