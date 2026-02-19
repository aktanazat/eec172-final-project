document.getElementById("year").textContent = new Date().getFullYear();

const topbar = document.querySelector(".topbar");
const revealTargets = document.querySelectorAll(
  ".tile, .module-detail, .process-card, .data-card"
);
const prefersReducedMotion = window.matchMedia(
  "(prefers-reduced-motion: reduce)"
).matches;

function syncTopbar() {
  if (!topbar) return;
  topbar.classList.toggle("scrolled", window.scrollY > 18);
}

syncTopbar();
window.addEventListener("scroll", syncTopbar, { passive: true });

if (!prefersReducedMotion && "IntersectionObserver" in window) {
  revealTargets.forEach((element, index) => {
    element.classList.add("reveal");
    element.style.setProperty("--reveal-delay", `${Math.min(index * 42, 260)}ms`);
  });

  const observer = new IntersectionObserver(
    (entries, instance) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) return;
        entry.target.classList.add("is-visible");
        instance.unobserve(entry.target);
      });
    },
    {
      threshold: 0.18,
      rootMargin: "0px 0px -8% 0px",
    }
  );

  revealTargets.forEach((element) => observer.observe(element));
} else {
  revealTargets.forEach((element) => element.classList.add("is-visible"));
}
