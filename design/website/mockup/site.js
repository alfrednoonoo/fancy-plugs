// Fancy Plugs website mockup: the phone menu, the home roster and the screenshot explorer.

// Phone menu: a <dialog>, so Esc closes it and focus returns to the button.
const sheet = document.querySelector("dialog.sheet");
const menuBtn = document.querySelector(".menu-btn");
if (sheet && menuBtn) {
  menuBtn.addEventListener("click", () => sheet.showModal());
  sheet.addEventListener("click", (e) => { if (e.target === sheet || e.target.closest(".close, a")) sheet.close(); });
}

// Home roster: choosing a name swaps the hero window, its caption and its link.
const roster = document.querySelector(".roster");
if (roster) {
  const img = document.querySelector(".hero figure img");
  const name = document.querySelector(".hero figcaption strong");
  const kind = document.querySelector(".hero figcaption span");
  const buttons = [...roster.querySelectorAll("button")];
  const choose = (b) => {
    buttons.forEach((x) => x.setAttribute("aria-pressed", String(x === b)));
    img.classList.add("out");
    const next = new Image();
    next.onload = () => {
      img.src = next.src; img.width = b.dataset.w; img.height = b.dataset.h;
      img.alt = "Fancy " + b.dataset.name + ", " + b.dataset.kind.toLowerCase();
      name.textContent = "Fancy " + b.dataset.name; kind.textContent = b.dataset.kind;
      img.classList.remove("out");
    };
    next.src = b.dataset.src;
  };
  buttons.forEach((b, i) => {
    b.addEventListener("click", () => choose(b));
    b.addEventListener("keydown", (e) => {
      const d = e.key === "ArrowRight" ? 1 : e.key === "ArrowLeft" ? -1 : 0;
      if (d) { const n = buttons[(i + d + buttons.length) % buttons.length]; n.focus(); n.scrollIntoView({ block: "nearest", inline: "nearest" }); }
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
      if (d) { const n = tabs[(i + d + tabs.length) % tabs.length]; show(n); n.focus(); }
    });
  });
  ex.querySelectorAll(".view").forEach((view) => {
    const set = (n, on) => view.querySelectorAll(`[data-n="${n}"]`).forEach((el) => el.classList.toggle("on", on));
    view.querySelectorAll("[data-n]").forEach((el) => {
      ["mouseenter", "focus"].forEach((ev) => el.addEventListener(ev, () => set(el.dataset.n, true)));
      ["mouseleave", "blur"].forEach((ev) => el.addEventListener(ev, () => set(el.dataset.n, false)));
      // Touch: a tap keeps one point lit.
      el.addEventListener("click", () => { view.querySelectorAll(".on").forEach((x) => x.classList.remove("on")); set(el.dataset.n, true); });
    });
  });
});
