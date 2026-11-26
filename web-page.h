// ======================================================================
//  web-page.h — САЙТ КЕРУВАННЯ РОБОТОМ
//
//  Це звичайна HTML-сторінка, яка зберігається в пам'яті плати.
//  R"rawliteral( ... )rawliteral" — це C++-спосіб записати довгий текст
//  без екранування лапок. Все, що між дужками, браузер отримує як є.
//
//  Щоб додати кнопку пози: знайди POSES нижче в <script> і додай рядок.
// ======================================================================
#pragma once
#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="uk">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Sesame</title>
<style>
  :root {
    --bg: #f4f1ec; --card: #ffffff; --text: #1f1d1a; --muted: #7a746b;
    --line: #e4ded4; --accent: #2f6f5e; --accent-soft: #e3efe9;
    --stop: #c2412d; --radius: 14px;
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --bg: #161614; --card: #22211e; --text: #eeeae3; --muted: #9a948a;
      --line: #34322d; --accent: #6cc3a6; --accent-soft: #263a33; --stop: #e26a55;
    }
  }
  * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
  body {
    margin: 0; background: var(--bg); color: var(--text);
    font: 16px/1.4 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
    user-select: none; -webkit-user-select: none;
  }
  main { max-width: 460px; margin: 0 auto; padding: 16px 16px 40px; }
  header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 14px; }
  h1 { font-size: 22px; margin: 0; letter-spacing: .5px; }
  .status { font-size: 13px; color: var(--muted); display: flex; align-items: center; gap: 6px; }
  .dot { width: 9px; height: 9px; border-radius: 50%; background: var(--muted); }
  .dot.on { background: var(--accent); }
  .dot.off { background: var(--stop); }

  section { background: var(--card); border: 1px solid var(--line); border-radius: var(--radius);
            padding: 14px; margin-bottom: 12px; }
  h2 { font-size: 13px; text-transform: uppercase; letter-spacing: 1px; color: var(--muted);
       margin: 0 0 10px; font-weight: 600; }

  button { font: inherit; color: var(--text); background: var(--bg); border: 1px solid var(--line);
           border-radius: 10px; padding: 12px 6px; cursor: pointer; touch-action: none; }
  button:active, button.held { background: var(--accent-soft); border-color: var(--accent); }

  .pad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
  .pad button { height: 70px; font-size: 26px; }
  .pad .stop { background: var(--stop); color: #fff; border: none; font-size: 15px; font-weight: 700; }

  .grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 8px; }
  .grid button { font-size: 14px; overflow-wrap: anywhere; }
  .grid button span { display: block; font-size: 22px; line-height: 1.2; }

  details { background: var(--card); border: 1px solid var(--line); border-radius: var(--radius);
            margin-bottom: 12px; }
  summary { padding: 14px; cursor: pointer; font-weight: 600; list-style: none; }
  summary::after { content: "+"; float: right; color: var(--muted); }
  details[open] summary::after { content: "–"; }
  .inner { padding: 0 14px 14px; }

  label { display: flex; justify-content: space-between; align-items: center; gap: 10px;
          margin: 8px 0; font-size: 15px; }
  input, select { font: inherit; color: var(--text); background: var(--bg); border: 1px solid var(--line);
                  border-radius: 8px; padding: 8px; }
  input[type=number] { width: 90px; text-align: right; }
  input[type=range] { flex: 1; padding: 0; accent-color: var(--accent); }
  .full { width: 100%; margin-top: 8px; }
  .primary { background: var(--accent); color: #fff; border: none; font-weight: 600; }
  .hint { font-size: 13px; color: var(--muted); margin: 6px 0 0; }
  .motor { display: grid; grid-template-columns: 34px 1fr 38px; align-items: center; gap: 8px; margin: 6px 0; }
  .motor b { font-size: 14px; }
  .motor output { text-align: right; font-variant-numeric: tabular-nums; font-size: 14px; }
  #toast { position: fixed; left: 50%; bottom: 20px; transform: translateX(-50%); background: var(--text);
           color: var(--bg); padding: 8px 14px; border-radius: 20px; font-size: 14px; opacity: 0;
           transition: opacity .2s; pointer-events: none; }
  #toast.show { opacity: 1; }
</style>
</head>
<body>
<main>
  <header>
    <h1>Sesame</h1>
    <div class="status"><span class="dot" id="dot"></span><span id="statusText">з'єднання…</span></div>
  </header>

  <section>
    <h2>Рух (тримай кнопку)</h2>
    <div class="pad">
      <div></div><button data-go="forward">▲</button><div></div>
      <button data-go="left">◀</button>
      <button class="stop" id="stopBtn">STOP</button>
      <button data-go="right">▶</button>
      <div></div><button data-go="backward">▼</button><div></div>
    </div>
    <p class="hint">З комп'ютера: стрілки або WASD, пробіл = стоп.</p>
  </section>

  <section>
    <h2>Пози</h2>
    <div class="grid" id="poses"></div>
  </section>

  <section>
    <h2>Обличчя</h2>
    <div class="grid" id="faces"></div>
  </section>

  <details id="settingsBox">
    <summary>Налаштування швидкості</summary>
    <div class="inner">
      <label>Пауза між кроками, мс <input type="number" id="frameDelay" min="10" max="2000"></label>
      <label>Кроків за раз <input type="number" id="walkCycles" min="1" max="100"></label>
      <label>Пауза між моторами, мс <input type="number" id="motorCurrentDelay" min="0" max="500"></label>
      <label>Анімація обличчя, кадр/с <input type="number" id="faceFps" min="1" max="30"></label>
      <button class="full primary" id="saveSettings">Зберегти</button>
      <p class="hint">Зберігається до перезавантаження. Назавжди — у config.h.</p>
    </div>
  </details>

  <details id="wifiBox">
    <summary>Wi-Fi (домашня мережа)</summary>
    <div class="inner">
      <p class="hint" id="wifiState">—</p>
      <button class="full" id="scanBtn">Знайти мережі</button>
      <select class="full" id="ssidList"><option value="">— спочатку натисни "Знайти" —</option></select>
      <input class="full" type="password" id="wifiPass" placeholder="Пароль">
      <button class="full primary" id="connectBtn">Підключити</button>
      <p class="hint">Під час підключення телефон може на кілька секунд втратити зв'язок з роботом. Це нормально.</p>
    </div>
  </details>

  <details id="motorBox">
    <summary>Ручне керування моторами</summary>
    <div class="inner" id="motors"></div>
  </details>
</main>
<div id="toast"></div>

<script>
// ---------- Списки кнопок (легко додати свою) ----------
const POSES = [
  ["stand","🧍","Стати"], ["rest","😴","Відпочинок"], ["wave","👋","Махати"],
  ["dance","💃","Танець"], ["swim","🏊","Плавати"], ["point","👉","Показати"],
  ["pushup","💪","Віджимання"], ["bow","🙇","Уклін"], ["cute","🥺","Милий"],
  ["freaky","🤪","Дивний"], ["worm","🐛","Хробак"], ["shake","🫨","Трусити"],
  ["shrug","🤷","Знизати"], ["dead","💀","Мертвий"], ["crab","🦀","Краб"]
];
const FACES = [
  ["happy","😊","Радість"], ["sad","😢","Сум"], ["angry","😠","Злість"],
  ["surprised","😮","Здивування"], ["sleepy","😪","Сонний"], ["love","😍","Любов"],
  ["excited","🤩","Захват"], ["confused","😕","Розгубленість"], ["thinking","🤔","Думає"]
];
const MOTORS = ["R1","R2","L1","L2","R4","R3","L3","L4"];

const $ = (id) => document.getElementById(id);

function toast(text) {
  const t = $("toast");
  t.textContent = text;
  t.classList.add("show");
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => t.classList.remove("show"), 1500);
}

function send(url) {
  return fetch(url).catch(() => toast("Немає зв'язку з роботом"));
}

// ---------- Рух ----------
let moving = null;
function move(dir) { if (moving === dir) return; moving = dir; send("/cmd?go=" + dir); }
function stop()    { moving = null; send("/cmd?stop=1"); }

document.querySelectorAll("[data-go]").forEach((btn) => {
  const dir = btn.dataset.go;
  btn.addEventListener("pointerdown", (e) => {
    btn.setPointerCapture(e.pointerId);
    btn.classList.add("held");
    move(dir);
  });
  const release = () => { if (btn.classList.contains("held")) { btn.classList.remove("held"); stop(); } };
  btn.addEventListener("pointerup", release);
  btn.addEventListener("pointercancel", release);
  btn.addEventListener("contextmenu", (e) => e.preventDefault());
});
$("stopBtn").addEventListener("click", stop);

const KEYS = { ArrowUp:"forward", w:"forward", ArrowDown:"backward", s:"backward",
               ArrowLeft:"left", a:"left", ArrowRight:"right", d:"right" };
document.addEventListener("keydown", (e) => {
  if (e.target.tagName === "INPUT" || e.target.tagName === "SELECT") return;
  if (e.key === " ") { e.preventDefault(); stop(); return; }
  const dir = KEYS[e.key];
  if (dir) { e.preventDefault(); move(dir); }
});
document.addEventListener("keyup", (e) => { if (KEYS[e.key] && moving === KEYS[e.key]) stop(); });

// ---------- Пози та обличчя ----------
function makeButtons(containerId, list, onClick) {
  const box = $(containerId);
  list.forEach(([name, emoji, label]) => {
    const b = document.createElement("button");
    b.innerHTML = "<span>" + emoji + "</span>" + label;
    b.addEventListener("click", () => onClick(name, label));
    box.appendChild(b);
  });
}
makeButtons("poses", POSES, (name, label) => { send("/cmd?pose=" + name); toast(label); });
makeButtons("faces", FACES, (name, label) => {
  fetch("/api/command", { method: "POST", headers: { "Content-Type": "application/json" },
                          body: JSON.stringify({ face: name }) })
    .then(() => toast(label)).catch(() => toast("Немає зв'язку з роботом"));
});

// ---------- Налаштування ----------
const SETTINGS = ["frameDelay", "walkCycles", "motorCurrentDelay", "faceFps"];
$("settingsBox").addEventListener("toggle", () => {
  if (!$("settingsBox").open) return;
  fetch("/getSettings").then((r) => r.json()).then((d) => {
    SETTINGS.forEach((k) => { $(k).value = d[k]; });
  }).catch(() => toast("Не вдалося прочитати"));
});
$("saveSettings").addEventListener("click", () => {
  const q = SETTINGS.map((k) => k + "=" + encodeURIComponent($(k).value)).join("&");
  send("/setSettings?" + q).then(() => toast("Збережено"));
});

// ---------- Мотори ----------
MOTORS.forEach((name, i) => {
  const row = document.createElement("div");
  row.className = "motor";
  row.innerHTML = "<b>" + name + "</b><input type='range' min='0' max='180' value='90'><output>90</output>";
  const slider = row.querySelector("input");
  const out = row.querySelector("output");
  let timer = null;
  slider.addEventListener("input", () => {
    out.textContent = slider.value;
    clearTimeout(timer);                       // не засипати робота запитами
    timer = setTimeout(() => send("/cmd?motor=" + (i + 1) + "&value=" + slider.value), 80);
  });
  $("motors").appendChild(row);
});

// ---------- Wi-Fi ----------
function showWifiStatus(d) {
  let text;
  if (d.connecting) text = "Підключаюсь…";
  else if (d.connected) text = "Підключено до " + d.ssid + " — відкривай http://" + d.ip +
                               (d.mdns ? " або http://" + d.host : "");
  else text = "Не підключено до домашньої мережі" + (d.lastError ? " (" + d.lastError + ")" : "");
  $("wifiState").textContent = text;
}
function checkWifi() {
  return fetch("/api/wifi/status").then((r) => r.json()).then((d) => { showWifiStatus(d); return d; });
}
$("wifiBox").addEventListener("toggle", () => { if ($("wifiBox").open) checkWifi().catch(() => {}); });

$("scanBtn").addEventListener("click", () => {
  $("scanBtn").textContent = "Шукаю…";
  const poll = () => fetch("/api/wifi/scan").then((r) => r.json()).then((d) => {
    if (!Array.isArray(d)) { setTimeout(poll, 700); return; }
    const best = {};
    d.forEach((n) => { if (n.ssid && (!best[n.ssid] || n.rssi > best[n.ssid].rssi)) best[n.ssid] = n; });
    const list = Object.values(best).sort((a, b) => b.rssi - a.rssi);
    const sel = $("ssidList");
    sel.innerHTML = "";
    list.forEach((n) => {
      const o = document.createElement("option");
      o.value = n.ssid;
      o.textContent = n.ssid + (n.secure ? " 🔒" : "") + "  (" + n.rssi + " dBm)";
      sel.appendChild(o);
    });
    if (!list.length) sel.innerHTML = "<option value=''>Мереж не знайдено</option>";
    $("scanBtn").textContent = "Знайти мережі";
  }).catch(() => { $("scanBtn").textContent = "Знайти мережі"; toast("Помилка пошуку"); });
  poll();
});

$("connectBtn").addEventListener("click", () => {
  const ssid = $("ssidList").value;
  if (!ssid) { toast("Спочатку вибери мережу"); return; }
  const body = "ssid=" + encodeURIComponent(ssid) + "&password=" + encodeURIComponent($("wifiPass").value);
  fetch("/api/wifi/connect", { method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" }, body: body })
    .then((r) => r.json()).then((d) => {
      if (!d.success) { toast(d.error || "Помилка"); return; }
      $("wifiState").textContent = "Підключаюсь…";
      const wait = () => checkWifi().then((s) => { if (s.connecting) setTimeout(wait, 1000); })
                                    .catch(() => setTimeout(wait, 1500));
      setTimeout(wait, 1000);
    }).catch(() => toast("Немає зв'язку з роботом"));
});

// ---------- Статус угорі ----------
function updateStatus() {
  fetch("/api/status").then((r) => r.json()).then((d) => {
    $("dot").className = "dot on";
    $("statusText").textContent = d.currentCommand ? "виконує: " + d.currentCommand : "онлайн";
  }).catch(() => {
    $("dot").className = "dot off";
    $("statusText").textContent = "немає зв'язку";
  });
}
updateStatus();
setInterval(updateStatus, 2000);
</script>
</body>
</html>
)rawliteral";
