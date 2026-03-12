const yearNode = document.getElementById("year");
if (yearNode) {
  yearNode.textContent = new Date().getFullYear();
}

const body = document.body;
const topbar = document.querySelector(".topbar");
const revealTargets = document.querySelectorAll(
  ".hero, .editorial-head, .mode-card, .process, .proof-card, .section, .card, .callout, .comparison-card, .meta-row > div, .protocol-grid article"
);
const prefersReducedMotion = window.matchMedia(
  "(prefers-reduced-motion: reduce)"
).matches;

let leavingPage = false;

if ("scrollRestoration" in window.history) {
  window.history.scrollRestoration = "manual";
}

if (window.location.hash) {
  window.history.replaceState(null, "", `${window.location.pathname}${window.location.search}`);
}

window.scrollTo(0, 0);

function enablePageTransition() {
  if (!body || prefersReducedMotion) return;

  body.classList.add("page-transition");
  requestAnimationFrame(() => {
    requestAnimationFrame(() => {
      body.classList.add("page-visible");
    });
  });
}

function isInternalPageLink(link, event) {
  if (event.defaultPrevented) return false;
  if (event.button !== 0) return false;
  if (event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) return false;
  if (link.target && link.target !== "_self") return false;
  if (link.hasAttribute("download")) return false;

  const href = link.getAttribute("href");
  if (!href) return false;
  if (href.startsWith("#") || href.startsWith("mailto:") || href.startsWith("tel:")) {
    return false;
  }

  const destination = new URL(href, window.location.href);
  if (destination.origin !== window.location.origin) return false;

  const currentPath = window.location.pathname.replace(/\/$/, "");
  const targetPath = destination.pathname.replace(/\/$/, "");

  if (currentPath === targetPath && destination.hash) {
    return false;
  }

  return true;
}

function installPageNavigationTransition() {
  if (!body || prefersReducedMotion) return;

  document.addEventListener("click", (event) => {
    const link = event.target.closest("a");
    if (!link || leavingPage) return;
    if (!isInternalPageLink(link, event)) return;

    const destination = new URL(link.getAttribute("href"), window.location.href);

    event.preventDefault();
    leavingPage = true;
    body.classList.add("page-leave");
    body.classList.remove("page-visible");

    window.setTimeout(() => {
      window.location.href = destination.href;
    }, 260);
  });
}

function installInPageScrollLinks() {
  document.addEventListener("click", (event) => {
    const link = event.target.closest("a");
    if (!link) return;

    const href = link.getAttribute("href");
    if (!href || !href.startsWith("#") || href.length < 2) return;

    const target = document.querySelector(href);
    if (!target) return;

    event.preventDefault();
    target.scrollIntoView({
      behavior: prefersReducedMotion ? "auto" : "smooth",
      block: "start",
    });
  });
}

function syncTopbar() {
  if (!topbar) return;
  topbar.classList.toggle("scrolled", window.scrollY > 12);
}

enablePageTransition();
installInPageScrollLinks();
installPageNavigationTransition();
syncTopbar();
window.addEventListener("scroll", syncTopbar, { passive: true });

revealTargets.forEach((element, index) => {
  element.classList.add("reveal");
  element.style.setProperty("--reveal-delay", `${Math.min(index * 32, 320)}ms`);
});

if (!prefersReducedMotion && "IntersectionObserver" in window) {
  const observer = new IntersectionObserver(
    (entries, instance) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) return;
        entry.target.classList.add("is-visible");
        instance.unobserve(entry.target);
      });
    },
    {
      threshold: 0.15,
      rootMargin: "0px 0px -8% 0px",
    }
  );

  revealTargets.forEach((element) => observer.observe(element));
} else {
  revealTargets.forEach((element) => element.classList.add("is-visible"));
}

const LIVE_BUCKET = "parkpilot-guidance-658947468902";
const LIVE_THING = "akge_cc3200_board";
const LOCAL_LEADERBOARD = "live/leaderboard-latest.json";
const LOCAL_REPLAY = "live/replay-latest.json";
const leaderboardPaths = [
  LOCAL_LEADERBOARD,
  `https://${LIVE_BUCKET}.s3.us-east-2.amazonaws.com/leaderboard/${LIVE_THING}/latest.json`,
  `https://${LIVE_BUCKET}.s3.amazonaws.com/leaderboard/${LIVE_THING}/latest.json`,
];

function formatTimestamp(unixSeconds) {
  if (!unixSeconds) return "--";
  return new Date(unixSeconds * 1000).toLocaleString();
}

function replayUrlFromKey(key, preferLocal) {
  if (preferLocal) return LOCAL_REPLAY;
  if (!key) return "#";
  return `https://${LIVE_BUCKET}.s3.us-east-2.amazonaws.com/${key}`;
}

async function fetchJsonWithFallback(urls) {
  let lastError = null;

  for (const url of urls) {
    try {
      const response = await fetch(url, { cache: "no-store" });
      if (!response.ok) {
        lastError = new Error(`HTTP ${response.status}`);
        continue;
      }
      const data = await response.json();
      return { data, url };
    } catch (error) {
      lastError = error;
    }
  }

  throw lastError || new Error("Unable to fetch JSON");
}

function setText(id, value) {
  const node = document.getElementById(id);
  if (node) node.textContent = value;
}

function renderLiveLeaderboard(payload, sourceUrl) {
  const runs = Array.isArray(payload.runs) ? payload.runs : [];
  const latest = runs[0] || null;
  const rows = document.getElementById("leaderboard-rows");
  const roundMeta = document.getElementById("live-round-meta");
  const heroMeta = document.getElementById("hero-run-meta");
  const replayLink = document.getElementById("live-replay-link");
  const leaderboardLink = document.getElementById("live-leaderboard-link");
  const preferLocalReplay = sourceUrl === LOCAL_LEADERBOARD;

  if (leaderboardLink) leaderboardLink.href = sourceUrl;

  if (!latest) {
    setText("live-winner", "NONE");
    setText("live-mission", "--");
    setText("hero-defender-score", "----");
    setText("hero-attacker-score", "----");
    if (rows) {
      rows.innerHTML = '<p class="live-empty">No completed rounds yet.</p>';
    }
    return;
  }

  setText("live-winner", latest.winner === "DEFENDER" ? "DEF" : "ATK");
  setText("live-mission", `M${latest.mission_level ?? "--"}`);
  setText("hero-defender-score", String(latest.defender_adjusted ?? 0).padStart(4, "0"));
  setText("hero-attacker-score", String(latest.attacker_adjusted ?? 0).padStart(4, "0"));

  if (roundMeta) {
    roundMeta.innerHTML = `
      <li>Updated: ${formatTimestamp(payload.updated_at)}</li>
      <li>Run: ${formatTimestamp(latest.timestamp)}</li>
      <li>Score: DEF ${latest.defender_adjusted} / ATK ${latest.attacker_adjusted}</li>
    `;
  }

  if (heroMeta) {
    heroMeta.innerHTML = `
      <li>Latest winner: ${latest.winner}</li>
      <li>Mission level: ${latest.mission_level}</li>
      <li>Replay and leaderboard are coming from S3</li>
    `;
  }

  if (replayLink) {
    replayLink.href = replayUrlFromKey(latest.replay_s3_key, preferLocalReplay);
  }

  if (rows) {
    rows.innerHTML = runs
      .slice(0, 6)
      .map(
        (run) => `
          <div class="leaderboard-row">
            <span>${new Date(run.timestamp * 1000).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}</span>
            <span>${run.winner === "DEFENDER" ? "DEF" : "ATK"}</span>
            <span>${run.defender_adjusted}-${run.attacker_adjusted}</span>
          </div>
        `
      )
      .join("");
  }
}

async function hydrateLiveCloudPanel() {
  const statusNode = document.getElementById("live-fetch-status");
  if (!statusNode) return;

  statusNode.textContent = "Loading S3 leaderboard...";

  try {
    const { data, url } = await fetchJsonWithFallback(leaderboardPaths);
    renderLiveLeaderboard(data, url);
    statusNode.textContent =
      url === LOCAL_LEADERBOARD
        ? "Loaded from the latest AWS snapshot bundled with the site"
        : `Live data loaded from ${url}`;
  } catch (error) {
    statusNode.textContent = `Live data unavailable in browser: ${error.message}`;
  }
}

hydrateLiveCloudPanel();
