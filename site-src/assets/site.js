// Fancy Plugs website. Every animation here is listed, with its reason, in docs/website-plan.md section 7.

const reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;
const saveData = !!(navigator.connection && navigator.connection.saveData);

// Demo clips (section 7.1): each loads when it's about to scroll into view, plays while at least
// half of it is on screen, and fades in over its screenshot once it's actually playing.
const clips = (() => {
  if (reduced || saveData || !("IntersectionObserver" in window)) return null;
  const seen = new Map();
  const load = (v) => {
    if (v.dataset.loaded === v.dataset.mp4 || !v.dataset.mp4) return;
    v.dataset.loaded = v.dataset.mp4;
    v.classList.remove("on");
    v.replaceChildren(...[["webm", "video/webm"], ["mp4", "video/mp4"]].map(([k, type]) => {
      const s = document.createElement("source"); s.src = v.dataset[k]; s.type = type; return s;
    }));
    v.load();
  };
  const sync = (v) => {
    const r = seen.get(v) || 0;
    if (r > 0) load(v);
    if (r >= 0.5 && v.dataset.loaded) v.play().catch(() => {}); else v.pause();
  };
  const io = new IntersectionObserver((es) => es.forEach((e) => { seen.set(e.target, e.intersectionRatio); sync(e.target); }),
    { rootMargin: "200px 0px", threshold: [0, 0.5, 1] });
  document.querySelectorAll("video.demo").forEach((v) => {
    v.addEventListener("playing", () => v.classList.add("on"));
    v.addEventListener("error", () => v.classList.remove("on"), true);
    io.observe(v);
  });
  return { load, sync };
})();

// Phone menu: a <dialog>, so Esc closes it and focus returns to the button.
const sheet = document.querySelector("dialog.sheet");
const menuBtn = document.querySelector(".menu-btn");
if (sheet && menuBtn) {
  menuBtn.addEventListener("click", () => sheet.showModal());
  sheet.addEventListener("click", (e) => { if (e.target === sheet || e.target.closest(".close, a")) sheet.close(); });
}

// Plugins menu: a <details>. Esc or a click outside closes it.
const plugmenu = document.querySelector(".plugmenu");
if (plugmenu) {
  document.addEventListener("click", (e) => { if (plugmenu.open && !plugmenu.contains(e.target)) plugmenu.open = false; });
  plugmenu.addEventListener("keydown", (e) => {
    if (e.key === "Escape" && plugmenu.open) { plugmenu.open = false; plugmenu.querySelector("summary").focus(); }
  });
}

// Home roster: choosing a name swaps the hero window, its caption and its link.
const roster = document.querySelector(".hero-roster");
if (roster) {
  const img = document.querySelector(".hero figure img");
  const shot = document.querySelector(".hero-shot");
  const video = shot && shot.querySelector("video.demo");
  const name = document.querySelector(".hero figcaption strong");
  const kind = document.querySelector(".hero figcaption .k");
  const link = document.querySelector(".hero figcaption a");
  const buttons = [...roster.querySelectorAll("button")];
  const choose = (b) => {
    buttons.forEach((x) => x.setAttribute("aria-pressed", String(x === b)));
    shot.classList.add("out");
    if (video) { video.pause(); video.classList.remove("on"); }
    const next = new Image();
    const leave = new Promise((r) => setTimeout(r, reduced ? 0 : 250));
    next.onload = () => leave.then(() => {
      img.srcset = b.dataset.srcset; img.src = b.dataset.src;
      img.width = b.dataset.w; img.height = b.dataset.h;
      img.alt = "Fancy " + b.dataset.name + ", " + b.dataset.kind.toLowerCase();
      name.textContent = "Fancy " + b.dataset.name; kind.textContent = b.dataset.kind;
      link.href = b.dataset.href; link.textContent = "See what Fancy " + b.dataset.name + " can do";
      // Jump to the far side without a transition, then slide in.
      shot.classList.remove("out"); shot.classList.add("enter");
      void shot.offsetWidth;
      shot.classList.remove("enter");
      if (video) {
        video.dataset.webm = b.dataset.webm; video.dataset.mp4 = b.dataset.mp4;
        if (!b.dataset.mp4) { video.removeAttribute("data-loaded"); video.replaceChildren(); video.load(); video.classList.remove("on"); }
        else if (clips) clips.sync(video);
      }
    });
    next.onerror = () => shot.classList.remove("out");
    next.src = b.dataset.src;
  };
  buttons.forEach((b, i) => {
    b.addEventListener("click", () => choose(b));
    b.addEventListener("keydown", (e) => {
      const d = e.key === "ArrowRight" ? 1 : e.key === "ArrowLeft" ? -1 : 0;
      if (d) { e.preventDefault(); const n = buttons[(i + d + buttons.length) % buttons.length]; n.focus(); n.scrollIntoView({ block: "nearest", inline: "nearest" }); }
    });
  });
}

// Screenshot explorer: tabs change the view; a marker and its list entry light up together.
document.querySelectorAll(".explorer").forEach((ex) => {
  const tabs = [...ex.querySelectorAll('[role="tab"]')];
  const show = (t) => {
    tabs.forEach((x) => {
      const on = x === t;
      x.setAttribute("aria-selected", String(on)); x.tabIndex = on ? 0 : -1;
      document.getElementById(x.getAttribute("aria-controls")).hidden = !on;
    });
  };
  tabs.forEach((t, i) => {
    t.addEventListener("click", () => show(t));
    t.addEventListener("keydown", (e) => {
      const d = e.key === "ArrowRight" ? 1 : e.key === "ArrowLeft" ? -1 : 0;
      if (d) { e.preventDefault(); const n = tabs[(i + d + tabs.length) % tabs.length]; show(n); n.focus(); }
    });
  });
  ex.querySelectorAll(".view").forEach((view) => {
    const set = (n, on) => view.querySelectorAll(`[data-n="${n}"]`).forEach((el) => el.classList.toggle("on", on));
    view.querySelectorAll("[data-n]").forEach((el) => {
      let timer;
      el.addEventListener("mouseenter", () => { timer = setTimeout(() => set(el.dataset.n, true), 150); });
      el.addEventListener("mouseleave", () => { clearTimeout(timer); set(el.dataset.n, false); });
      el.addEventListener("focus", () => set(el.dataset.n, true));
      el.addEventListener("blur", () => set(el.dataset.n, false));
      // Touch: a tap keeps one point lit.
      el.addEventListener("click", () => { view.querySelectorAll(".on").forEach((x) => x.classList.remove("on")); set(el.dataset.n, true); });
    });
  });
});

// Hear it: one clip pair. Before and After switch source without losing the position.
document.querySelectorAll(".player").forEach((pl) => {
  const audio = { before: pl.querySelector("audio[data-v='before']"), after: pl.querySelector("audio[data-v='after']") };
  const play = pl.querySelector(".play");
  const segs = [...pl.querySelectorAll(".seg button")];
  const bar = pl.querySelector(".bar"), fill = bar.querySelector("i");
  const time = pl.querySelector(".time"), status = pl.querySelector(".status");
  let cur = "after";
  const fmt = (t) => Math.floor(t / 60) + ":" + String(Math.floor(t % 60)).padStart(2, "0");
  const a = () => audio[cur];
  const tick = () => {
    const x = a(); if (!x.duration) return;
    fill.style.width = (100 * x.currentTime / x.duration) + "%";
    time.textContent = fmt(x.currentTime) + " / " + fmt(x.duration);
  };
  Object.values(audio).forEach((x) => {
    x.addEventListener("timeupdate", tick);
    x.addEventListener("loadedmetadata", tick);
    x.addEventListener("waiting", () => { status.textContent = "Loading clip"; });
    x.addEventListener("playing", () => { status.textContent = ""; });
    x.addEventListener("ended", () => { play.setAttribute("aria-pressed", "false"); play.setAttribute("aria-label", "Play"); });
    x.addEventListener("error", () => {
      status.innerHTML = 'This clip didn’t load. <button type="button" class="btn btn-2">Try again</button>';
      status.querySelector("button").addEventListener("click", () => { status.textContent = ""; Object.values(audio).forEach((y) => y.load()); });
    });
  });
  play.addEventListener("click", () => {
    const on = play.getAttribute("aria-pressed") !== "true";
    play.setAttribute("aria-pressed", String(on)); play.setAttribute("aria-label", on ? "Pause" : "Play");
    if (on) { status.textContent = "Loading clip"; a().play().then(() => { status.textContent = ""; }).catch(() => { status.textContent = "This clip didn’t load."; }); }
    else a().pause();
  });
  segs.forEach((s) => s.addEventListener("click", () => {
    if (s.dataset.v === cur) return;
    const from = a(), to = audio[s.dataset.v], playing = !from.paused;
    segs.forEach((x) => x.setAttribute("aria-pressed", String(x === s)));
    cur = s.dataset.v;
    to.currentTime = from.currentTime;
    if (playing) {
      // A 30 ms crossfade, so the switch doesn't click.
      to.volume = 0; to.play();
      const t0 = performance.now();
      const step = (t) => {
        const k = Math.min(1, (t - t0) / 30);
        to.volume = k; from.volume = 1 - k;
        if (k < 1) requestAnimationFrame(step); else { from.pause(); from.volume = 1; }
      };
      requestAnimationFrame(step);
    }
    tick();
  }));
  bar.addEventListener("click", (e) => {
    const r = bar.getBoundingClientRect(), x = a();
    if (x.duration) { x.currentTime = x.duration * (e.clientX - r.left) / r.width; tick(); }
  });
});

// Copy buttons on the download page.
// execCommand first: it answers at once, where the Clipboard API can wait on a permission prompt.
document.querySelectorAll("[data-copy]").forEach((b) => b.addEventListener("click", () => {
  const done = (ok) => { b.textContent = ok ? "Copied" : "Select and copy"; setTimeout(() => { b.textContent = "Copy"; }, 2000); };
  const t = document.createElement("textarea");
  t.value = b.dataset.copy; t.setAttribute("readonly", ""); t.style.position = "fixed"; t.style.opacity = "0";
  document.body.appendChild(t); t.select();
  let ok = false;
  try { ok = document.execCommand("copy"); } catch (e) { ok = false; }
  t.remove(); b.focus();
  if (ok || !navigator.clipboard) return done(ok);
  // A Clipboard API that never answers (a pending permission prompt) gives up after 1.5 s.
  let answered = false;
  const once = (v) => { if (!answered) { answered = true; done(v); } };
  navigator.clipboard.writeText(b.dataset.copy).then(() => once(true), () => once(false));
  setTimeout(() => once(false), 1500);
}));

// Scroll reveal, once each: screenshots rise and grow slightly, headings and text blocks rise.
// The hero is left alone, so the first screen is there at once. Without JavaScript nothing is hidden.
if (!reduced && "IntersectionObserver" in window) {
  document.querySelectorAll("main h2, .group-head p, .shared .lede, .shared-list li, .lead > div:first-child, .split > div:first-child, .can li, .pairs a, .tile h3, .tile p")
    .forEach((el) => { if (!el.closest(".hero, .reveal")) el.classList.add("reveal-text"); });
  const items = document.querySelectorAll(".reveal, .reveal-text");
  if (items.length) {
    document.documentElement.classList.add("js-reveal");
    const io = new IntersectionObserver((es) => es.forEach((e) => {
      if (e.isIntersecting) { e.target.classList.add("in"); io.unobserve(e.target); }
    }), { rootMargin: "0px 0px -8% 0px" });
    items.forEach((el) => io.observe(el));
  }
}

// Guide: the side list marks the section being read.
// The current section is the last heading above a third of the window; at the bottom of the page,
// the last one, since the final sections can't scroll that high.
const toc = document.querySelector(".toc");
if (toc) {
  const links = [...toc.querySelectorAll("a")];
  const heads = links.map((a) => document.getElementById(a.getAttribute("href").slice(1)));
  // Eleven headings: cheap enough to measure on every scroll event.
  const update = () => {
    const atEnd = innerHeight + scrollY >= document.documentElement.scrollHeight - 4;
    let i = atEnd ? heads.length - 1 : -1;
    if (!atEnd) heads.forEach((h, k) => { if (h && h.getBoundingClientRect().top < innerHeight / 3) i = k; });
    links.forEach((a, k) => (k === i ? a.setAttribute("aria-current", "true") : a.removeAttribute("aria-current")));
  };
  addEventListener("scroll", update, { passive: true });
  addEventListener("load", update);
  update();
}
