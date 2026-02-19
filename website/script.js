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
