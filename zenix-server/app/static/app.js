// Zenix AI console: SPA nho, khong can build. Router theo hash: #/devices, #/agents, #/agents/:id ...
let lang = localStorage.getItem('lang') || (navigator.language.startsWith('vi') ? 'vi' : 'en');
let me = null;
const app = document.getElementById('app');

const t = (k) => (I18N[lang] && I18N[lang][k]) || I18N.en[k] || k;
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

// Bo icon net (stroke) 24x24.
const ICONS = {
  device: '<rect x="6" y="3" width="12" height="18" rx="4"/><circle cx="12" cy="10" r="2.5"/><path d="M9.5 16.5h5"/>',
  agent: '<path d="M12 3l1.8 4.7L18.5 9.5l-4.7 1.8L12 16l-1.8-4.7L5.5 9.5l4.7-1.8z"/><path d="M19 15l.8 2.2L22 18l-2.2.8L19 21l-.8-2.2L16 18l2.2-.8z"/>',
  settings: '<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/>',
  users: '<path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M22 21v-2a4 4 0 0 0-3-3.9M16 3.1a4 4 0 0 1 0 7.8"/>',
  logout: '<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><path d="M16 17l5-5-5-5M21 12H9"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  chat: '<path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>',
  edit: '<path d="M12 20h9"/><path d="M16.5 3.5a2.1 2.1 0 1 1 3 3L7 19l-4 1 1-4z"/>',
  trash: '<path d="M3 6h18M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/>',
  unlink: '<path d="M18.8 13.4l1.8-1.8a4.6 4.6 0 0 0-6.5-6.5l-1.8 1.8M5.2 10.6l-1.8 1.8a4.6 4.6 0 0 0 6.5 6.5l1.8-1.8M8 2v3M2 8h3M16 22v-3M22 16h-3"/>',
  wifi: '<path d="M5 12.6a10 10 0 0 1 14 0M8.5 16.1a5 5 0 0 1 7 0M2 8.8a15 15 0 0 1 20 0"/><circle cx="12" cy="20" r="1"/>',
  back: '<path d="M19 12H5M12 19l-7-7 7-7"/>',
  key: '<circle cx="7.5" cy="15.5" r="5.5"/><path d="M11.4 11.6L21 2M16 7l3 3M18 5l3 3"/>',
  alert: '<path d="M10.3 3.9L1.8 18a2 2 0 0 0 1.7 3h17a2 2 0 0 0 1.7-3L13.7 3.9a2 2 0 0 0-3.4 0zM12 9v4M12 17h.01"/>',
  mic: '<rect x="9" y="2" width="6" height="12" rx="3"/><path d="M19 10v1a7 7 0 0 1-14 0v-1M12 18v4"/>',
  globe: '<circle cx="12" cy="12" r="10"/><path d="M2 12h20M12 2a15 15 0 0 1 0 20 15 15 0 0 1 0-20"/>',
  cpu: '<rect x="4" y="4" width="16" height="16" rx="3"/><rect x="9" y="9" width="6" height="6"/><path d="M9 1v3M15 1v3M9 20v3M15 20v3M20 9h3M20 14h3M1 9h3M1 14h3"/>',
  card: '<rect x="2" y="5" width="20" height="14" rx="3"/><path d="M2 10h20M6 15h4"/>',
  lock: '<rect x="3" y="11" width="18" height="11" rx="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/>',
};
const icon = (name) => `<svg class="i" viewBox="0 0 24 24" aria-hidden="true">${ICONS[name]}</svg>`;
const initial = (s) => esc((s || '?').trim().charAt(0).toUpperCase());

async function api(path, opts = {}) {
  const res = await fetch('/api' + path, {
    method: opts.method || (opts.body ? 'POST' : 'GET'),
    headers: opts.body ? { 'Content-Type': 'application/json' } : {},
    body: opts.body ? JSON.stringify(opts.body) : undefined,
  });
  const data = await res.json().catch(() => ({}));
  if (res.status === 401 && path !== '/auth/login') { me = null; route(); throw new Error('401'); }
  if (!res.ok) throw new Error(typeof data.detail === 'string' ? data.detail : t('error'));
  return data;
}

function toast(msg, err = false) {
  const el = document.createElement('div');
  if (err) el.className = 'err';
  el.textContent = I18N[lang][msg] || msg;
  document.getElementById('toast').appendChild(el);
  setTimeout(() => el.remove(), 3500);
}

function fmtTime(ts) {
  if (!ts) return t('never');
  return new Date(ts * 1000).toLocaleString(lang === 'vi' ? 'vi-VN' : 'en-US', { dateStyle: 'short', timeStyle: 'short' });
}

function formData(form) {
  return Object.fromEntries(new FormData(form).entries());
}

function setLang(l) {
  lang = l; localStorage.setItem('lang', lang); document.documentElement.lang = lang; route();
}

const langSeg = () => `<div class="seg">${['vi', 'en'].map((l) => `<button type="button" data-lang="${l}" class="${l === lang ? 'on' : ''}">${l.toUpperCase()}</button>`).join('')}</div>`;

const pageHead = (title, sub, action = '', crumb = '') => `
  <div class="page-head"><div>${crumb}<h1>${title}</h1>${sub ? `<p>${sub}</p>` : ''}</div>${action}</div>`;

const crumbLink = (href) => `<a class="crumb" href="${href}">${icon('back')}${t('back')}</a>`;

const emptyState = (ic, title, sub, action) =>
  `<div class="card empty"><span class="avatar">${icon(ic)}</span><h2>${title}</h2><p>${sub}</p>${action}</div>`;

// ---------- khung trang ----------

function shell(active, inner) {
  const links = [['devices', 'device'], ['agents', 'agent'], ['keys', 'key']];
  if (me.role === 'admin') links.push(['plans', 'card'], ['users', 'users'], ['settings', 'settings']);
  app.innerHTML = `
    <div class="layout">
      <aside class="side">
        <a class="brand" href="#/devices"><img src="/static/logo.png" alt=""><div>Zenix AI<small>${t('console')}</small></div></a>
        <nav class="nav">${links.map(([r, ic]) => `<a href="#/${r}" class="${active === r ? 'on' : ''}">${icon(ic)}${t(r)}</a>`).join('')}</nav>
        <div class="side-foot">
          ${langSeg()}
          <div class="me">
            <span class="avatar sm">${initial(me.username)}</span>
            <div class="who"><b>${esc(me.username)}</b><span>${t('role_' + me.role)}</span></div>
            <button class="icon-btn" id="pw" title="${t('change_password')}">${icon('lock')}</button>
            <button class="icon-btn" id="logout" title="${t('logout')}">${icon('logout')}</button>
          </div>
        </div>
      </aside>
      <div class="content"><main>${inner}</main></div>
    </div>`;
  app.querySelectorAll('[data-lang]').forEach((b) => b.onclick = () => setLang(b.dataset.lang));
  document.getElementById('logout').onclick = async () => { await api('/auth/logout', { body: {} }); me = null; route(); };
  document.getElementById('pw').onclick = changePassword;
}

function modal(html, onSubmit) {
  const wrap = document.createElement('div');
  wrap.className = 'backdrop';
  wrap.innerHTML = `<form class="modal">${html}</form>`;
  document.body.appendChild(wrap);
  const form = wrap.querySelector('form');
  const close = () => wrap.remove();
  wrap.addEventListener('click', (e) => { if (e.target === wrap) close(); });
  form.querySelector('[data-cancel]')?.addEventListener('click', close);
  form.onsubmit = async (e) => {
    e.preventDefault();
    try { if ((await onSubmit(formData(form))) !== false) close(); } catch (err) { toast(err.message, true); }
  };
  form.querySelector('input, select, textarea')?.focus();
  return form;
}

const modalActions = (submitLabel) =>
  `<div class="row-actions"><button type="button" class="btn ghost" data-cancel>${t('cancel')}</button><button class="btn">${submitLabel}</button></div>`;

function changePassword() {
  modal(`<h2>${t('change_password')}</h2><p></p>
    <label class="f"><span>${t('old_password')}</span><input type="password" name="old_password" required></label>
    <label class="f"><span>${t('new_password')}</span><input type="password" name="new_password" minlength="8" required></label>
    ${modalActions(t('save'))}`,
    async (d) => { await api('/auth/password', { body: d }); toast('saved'); });
}

// ---------- dang nhap / thiet lap lan dau ----------

let authInfo = { allow_registration: false };

function authScreen(mode) {
  // mode: 'setup' (tai khoan dau tien) | 'login' | 'register'
  const setup = mode === 'setup', reg = mode === 'register';
  const title = setup ? t('setup_title') : reg ? t('create_account') : t('welcome');
  const sub = setup ? t('setup_sub') : reg ? t('register_sub') : t('login_sub');
  const switchLink = setup || !authInfo.allow_registration ? '' : `<div class="auth-switch">${reg ? t('have_account') : t('no_account')}
    <a data-mode="${reg ? 'login' : 'register'}">${reg ? t('login') : t('register')}</a></div>`;
  app.innerHTML = `<div class="auth">
    <section class="auth-art">
      <div class="logo"><img src="/static/logo.png" alt="">Zenix AI</div>
      <div>
        <h2>${t('hero_title')}</h2>
        <p>${t('hero_sub')}</p>
        <ul><li>${icon('globe')}${t('hero_1')}</li><li>${icon('agent')}${t('hero_2')}</li><li>${icon('mic')}${t('hero_3')}</li></ul>
      </div>
      <small style="opacity:.7">© ${new Date().getFullYear()} Zenix</small>
    </section>
    <section class="auth-form"><form id="f">
      <h1>${title}</h1>
      <p>${sub}</p>
      <label class="f"><span>${t('username')}</span><input name="username" autocomplete="username" required minlength="3" maxlength="40"></label>
      <label class="f"><span>${t('password')}</span><input type="password" name="password" autocomplete="${setup || reg ? 'new-password' : 'current-password'}" required minlength="8"></label>
      <button class="btn">${setup || reg ? t('create') : t('login')}</button>
      ${switchLink}
      ${langSeg()}
    </form></section>
  </div>`;
  app.querySelectorAll('[data-lang]').forEach((b) => b.onclick = () => { lang = b.dataset.lang; localStorage.setItem('lang', lang); authScreen(mode); });
  app.querySelector('[data-mode]')?.addEventListener('click', (e) => authScreen(e.target.dataset.mode));
  document.getElementById('f').onsubmit = async (e) => {
    e.preventDefault();
    try {
      await api(setup ? '/auth/setup' : reg ? '/auth/register' : '/auth/login', { body: formData(e.target) });
      route();
    } catch (err) { toast(err.message, true); }
  };
}

// ---------- thiet bi ----------

function statusPill(d) {
  if (!d.online) return `<span class="pill">${t('offline')}</span>`;
  const st = d.state || 'idle';
  return `<span class="pill ${st === 'idle' ? 'ok' : 'live'}">${t('state_' + st)}</span>`;
}

async function devicesPage() {
  const [devices, agents] = await Promise.all([api('/devices'), api('/agents')]);
  const agentOptions = (sel) => `<option value="">${t('default_agent')}</option>` +
    agents.map((a) => `<option value="${a.id}" ${a.id === sel ? 'selected' : ''}>${esc(a.name)}</option>`).join('');
  const online = devices.filter((d) => d.online).length;
  const addBtn = (id) => `<button class="btn" id="${id}">${icon('plus')}${t('add_device')}</button>`;
  shell('devices', `
    ${pageHead(t('devices'), t('devices_sub'), addBtn('add'))}
    <div class="stats">
      <div class="stat hero"><span>${t('stat_devices')}</span><b>${devices.length}</b>${icon('device')}</div>
      <div class="stat"><span>${t('stat_online')}</span><b>${online}</b>${icon('wifi')}</div>
      <div class="stat"><span>${t('stat_agents')}</span><b>${agents.length}</b>${icon('agent')}</div>
    </div>
    ${devices.length ? `<div class="grid">${devices.map((d) => `
      <div class="card item">
        <div class="item-head">
          <span class="avatar ${d.online ? '' : 'muted'}">${icon('device')}</span>
          <div class="t"><b>${esc(d.name || 'Zin')}</b><small>${esc(d.mac)}</small></div>
          ${statusPill(d)}
        </div>
        <div class="kv">
          <span>${t('firmware')}</span><span>${esc(d.firmware || '–')}${d.board ? ' · ' + esc(d.board) : ''}</span>
          <span>IP</span><span>${esc(d.last_ip || '–')}</span>
          <span>${t('last_seen')}</span><span>${d.online ? t('online') : fmtTime(d.last_seen)}</span>
        </div>
        <label class="f grow"><span>${t('agent')}</span><select data-agent="${d.id}">${agentOptions(d.agent_id)}</select></label>
        <div class="item-foot">
          <a class="btn ghost sm" href="#/devices/${d.id}">${icon('chat')}${t('history')}</a>
          <button class="btn ghost sm" data-rename="${d.id}" data-name="${esc(d.name)}">${icon('edit')}${t('rename')}</button>
          <button class="btn danger sm" data-unbind="${d.id}" style="margin-left:auto">${icon('unlink')}${t('unbind')}</button>
        </div>
      </div>`).join('')}</div>`
    : emptyState('device', t('no_devices'), t('no_devices_sub'), addBtn('add2'))}`);

  const openBind = () => {
    if (!agents.length) toast('no_agent_warn', true);
    modal(`<h2>${t('bind_title')}</h2><p>${t('bind_help')}</p>
      <label class="f"><span>${t('code')}</span><input name="code" class="code-input" inputmode="numeric" pattern="\\d{6}" maxlength="6" required autocomplete="off" placeholder="••••••"></label>
      <label class="f"><span>${t('agent')}</span><select name="agent_id">${agentOptions(agents[0]?.id)}</select></label>
      <label class="f"><span>${t('device_name')}</span><input name="name" placeholder="Zin"></label>
      ${modalActions(t('bind'))}`,
      async (d) => {
        await api('/devices/bind', { body: { code: d.code, name: d.name, agent_id: d.agent_id ? +d.agent_id : null } });
        toast('bound_ok'); devicesPage();
      });
  };
  document.getElementById('add').onclick = openBind;
  document.getElementById('add2')?.addEventListener('click', openBind);
  app.querySelectorAll('[data-agent]').forEach((s) => s.onchange = async () => {
    await api(`/devices/${s.dataset.agent}`, { method: 'PUT', body: { agent_id: s.value ? +s.value : null } });
    toast('saved');
  });
  app.querySelectorAll('[data-rename]').forEach((b) => b.onclick = () => modal(
    `<h2>${t('rename')}</h2><p></p><label class="f"><span>${t('device_name')}</span><input name="name" value="${b.dataset.name}" maxlength="60"></label>
     ${modalActions(t('save'))}`,
    async (d) => { await api(`/devices/${b.dataset.rename}`, { method: 'PUT', body: { name: d.name } }); devicesPage(); }));
  app.querySelectorAll('[data-unbind]').forEach((b) => b.onclick = async () => {
    if (!confirm(t('confirm_unbind'))) return;
    await api(`/devices/${b.dataset.unbind}`, { method: 'DELETE' }); devicesPage();
  });
}

async function deviceHistoryPage(id) {
  const msgs = await api(`/devices/${id}/messages`);
  shell('devices', `
    ${pageHead(t('conversation'), '', `<button class="btn danger" id="clear">${icon('trash')}${t('clear_history')}</button>`, crumbLink('#/devices'))}
    ${msgs.length ? `<div class="card"><div class="chat">${msgs.map((m) =>
      `<div class="bubble ${m.role}">${esc(m.content)}<time>${fmtTime(m.created_at)}</time></div>`).join('')}</div></div>`
      : emptyState('chat', t('no_messages'), '', '')}`);
  document.getElementById('clear').onclick = async () => {
    if (!confirm(t('clear_history') + '?')) return;
    await api(`/devices/${id}/messages`, { method: 'DELETE' }); deviceHistoryPage(id);
  };
}

// ---------- gia & bo chon mo hinh ----------
// Moi muc trong danh muc co price_in/price_out va unit: token ($/1M token), minute ($/phut), mchar ($/1M ky tu).

function fmtUsd(v) {
  if (v == null) return '–';
  if (v === 0) return t('free');
  if (v >= 0.01) return '$' + v.toFixed(2);
  return '$' + v.toPrecision(2);
}
const fmtCtx = (n) => !n ? '' : n >= 1e6 ? `${+(n / 1e6).toFixed(1)}M` : `${Math.round(n / 1000)}K`;
const isFree = (m) => m && m.price_in === 0 && m.price_out === 0;
const hasPrice = (m) => m && m.price_in != null;

function priceText(m) {
  if (!hasPrice(m)) return '';
  if (isFree(m)) return t('free');
  const approx = m.approx ? ' ≈' : '';
  if (m.unit === 'minute') return `${fmtUsd(m.price_in)} / ${t('minute')}${approx}`;
  if (m.unit === 'mchar') return `${fmtUsd(m.price_in)} / ${t('per_mchar')}${approx}`;
  return `${t('price_in')} ${fmtUsd(m.price_in)} · ${t('price_out')} ${fmtUsd(m.price_out)} ${t('per_m')}${approx}`;
}

// Chi phi uoc tinh cho mot luot hoi thoai: LLM ~1.500 token vao + 150 ra; STT ~5 giay noi;
// TTS ~150 ky tu tra loi (~10 giay am thanh ~ 250 token).
function turnCost(m, kind) {
  if (!hasPrice(m)) return null;
  if (m.unit === 'minute') return m.price_in * 5 / 60;
  if (m.unit === 'mchar') return m.price_in * 150 / 1e6;
  const [tin, tout] = kind === 'llm' ? [1500, 150] : kind === 'stt' ? [100, 20] : [40, 250];
  return (m.price_in * tin + m.price_out * tout) / 1e6;
}
// Hien theo 1.000 luot de so sanh de dang (tranh so qua nho).
const fmtTurn = (c) => c == null ? '' : c === 0 ? t('free') : `≈ ${fmtUsd(c * 1000)} / ${t('per_1k_turns')}`;

const catCache = {};
function catalog(kind, provider = 'openrouter', planId = null) {
  const key = `${kind}|${provider}|${planId || ''}`;
  if (!catCache[key]) {
    catCache[key] = api(`/models/${kind}?provider=${provider}${planId ? `&plan_id=${planId}` : ''}`)
      .catch((e) => { delete catCache[key]; throw e; });
  }
  return catCache[key];
}
const clearCatalog = (provider) => Object.keys(catCache).forEach((k) => { if (k.includes(`|${provider}|`)) delete catCache[k]; });

const CHEAP = { llm: 0.0005, stt: 0.0005, tts: 0.003 };  // nguong "re" theo chi phi moi luot

// Hop thoai chon model (llm | stt | tts). Tra ve id hoac null.
function pickModel({ kind, provider = 'openrouter', planId = null, current = '' }) {
  return new Promise((resolve) => {
    const filters = kind === 'llm' ? ['all', 'free', 'cheap', 'tools'] : ['all', 'free', 'cheap'];
    const wrap = document.createElement('div');
    wrap.className = 'backdrop';
    wrap.innerHTML = `<div class="modal wide">
      <div class="picker-head"><h2>${t('choose_' + kind)}</h2>
        <input id="mq" placeholder="${t('search_models')}" autocomplete="off">
        <div class="picker-tools"><div class="chips" id="mf">${filters.map((f, i) =>
          `<button type="button" data-f="${f}" class="${i ? '' : 'on'}">${t('f_' + f)}</button>`).join('')}</div>
          <select id="ms"><option value="price">${t('sort_price')}</option><option value="name">${t('sort_name')}</option></select></div>
      </div>
      <div class="picker-list" id="ml"><div class="empty">${t('loading')}</div></div>
      <div class="picker-foot"><span id="mc"></span><span class="muted">${t('turn_hint_' + kind)}</span><button type="button" class="btn ghost sm" data-cancel>${t('cancel')}</button></div>
    </div>`;
    document.body.appendChild(wrap);
    const onKey = (e) => { if (e.key === 'Escape') close(null); };
    const close = (val) => { document.removeEventListener('keydown', onKey); wrap.remove(); resolve(val); };
    document.addEventListener('keydown', onKey);
    wrap.addEventListener('click', (e) => { if (e.target === wrap) close(null); });
    wrap.querySelector('[data-cancel]').onclick = () => close(null);
    let all = [], filter = 'all';
    const render = () => {
      const q = wrap.querySelector('#mq').value.trim().toLowerCase();
      const sort = wrap.querySelector('#ms').value;
      let list = all.filter((m) => !q || m.id.toLowerCase().includes(q) || (m.name || '').toLowerCase().includes(q));
      if (filter === 'free') list = list.filter(isFree);
      if (filter === 'cheap') list = list.filter((m) => { const c = turnCost(m, kind); return c != null && c < CHEAP[kind]; });
      if (filter === 'tools') list = list.filter((m) => m.tools);
      const cost = (m) => turnCost(m, kind) ?? Infinity;
      list.sort(sort === 'price' ? (a, b) => cost(a) - cost(b) || a.id.localeCompare(b.id) : (a, b) => a.id.localeCompare(b.id));
      wrap.querySelector('#mc').textContent = `${list.length} ${t('models_count')}`;
      wrap.querySelector('#ml').innerHTML = list.length ? list.slice(0, 300).map((m) => `
        <button type="button" class="mrow ${m.id === current ? 'on' : ''}" data-id="${esc(m.id)}">
          <div class="nm"><b>${esc(m.name || m.id)}</b><small>${esc(m.id)}</small></div>
          <div class="pr">${hasPrice(m) ? `<b>${esc(fmtTurn(turnCost(m, kind)))}</b><br><span>${esc(priceText(m))}</span>` : '<span>–</span>'}</div>
          <div class="tags">${isFree(m) ? `<span class="tag green">${t('free')}</span>` : ''}${m.tools ? '<span class="tag blue">tools</span>' : ''}${m.context ? `<span class="tag">${fmtCtx(m.context)} ${t('ctx')}</span>` : ''}${m.voices && m.voices.length ? `<span class="tag">${m.voices.length} ${t('voices')}</span>` : ''}${m.approx && hasPrice(m) ? `<span class="tag" title="${t('approx_price')}">≈ OpenRouter</span>` : ''}</div>
        </button>`).join('') : `<div class="empty">${t('no_models')}</div>`;
      wrap.querySelectorAll('.mrow').forEach((b) => b.onclick = () => close(b.dataset.id));
    };
    wrap.querySelector('#mq').oninput = render;
    wrap.querySelector('#ms').onchange = render;
    wrap.querySelectorAll('#mf button').forEach((b) => b.onclick = () => {
      filter = b.dataset.f; wrap.querySelectorAll('#mf button').forEach((x) => x.classList.toggle('on', x === b)); render();
    });
    wrap.querySelector('#mq').focus();
    catalog(kind, provider, planId).then((ms) => { all = ms; render(); })
      .catch((e) => { wrap.querySelector('#ml').innerHTML = `<div class="empty">${esc(I18N[lang][e.message] || e.message)}</div>`; });
  });
}

// Hop thoai chon giong (danh sach chuoi). Tra ve giong hoac null; '' = tu chon theo ngon ngu.
function pickVoice(voices, current) {
  return new Promise((resolve) => {
    const wrap = document.createElement('div');
    wrap.className = 'backdrop';
    wrap.innerHTML = `<div class="modal wide">
      <div class="picker-head"><h2>${t('choose_voice')}</h2><input id="vq" placeholder="${t('search_voices')}" autocomplete="off"></div>
      <div class="picker-list" id="vl"></div>
      <div class="picker-foot"><span>${voices.length} ${t('voices')}</span><button type="button" class="btn ghost sm" data-cancel>${t('cancel')}</button></div>
    </div>`;
    document.body.appendChild(wrap);
    const close = (val) => { wrap.remove(); resolve(val); };
    wrap.addEventListener('click', (e) => { if (e.target === wrap) close(null); });
    wrap.querySelector('[data-cancel]').onclick = () => close(null);
    const render = () => {
      const q = wrap.querySelector('#vq').value.trim().toLowerCase();
      const list = ['', ...voices.filter((v) => !q || v.toLowerCase().includes(q))];
      wrap.querySelector('#vl').innerHTML = list.map((v) => `
        <button type="button" class="mrow ${v === current ? 'on' : ''}" data-v="${esc(v)}">
          <div class="nm"><b>${v ? esc(v) : t('voice_auto')}</b><small>${v ? '' : t('voice_auto_hint')}</small></div></button>`).join('');
      wrap.querySelectorAll('.mrow').forEach((b) => b.onclick = () => close(b.dataset.v));
    };
    wrap.querySelector('#vq').oninput = render;
    render();
    wrap.querySelector('#vq').focus();
  });
}

// Noi dung nut hien lua chon hien tai (ten + gia).
async function modelBtnHtml({ kind, provider = 'openrouter', planId = null, id, fallbackId, fallbackLabel }) {
  const show = id || fallbackId;
  let info = null;
  if (show) { try { info = (await catalog(kind, provider, planId)).find((m) => m.id === show); } catch { /* chua co key */ } }
  const name = show ? (info?.name || show) : t('choose_model');
  const sub = [id ? '' : fallbackLabel, priceText(info), fmtTurn(turnCost(info, kind))].filter(Boolean).join(' · ');
  return `<div class="nm"><b>${esc(name)}</b><small>${esc(sub || show || '')}</small></div>${icon('edit')}`;
}

const modelInput = (name, value) =>
  `<input type="hidden" name="${name}" value="${esc(value || '')}"><button type="button" class="model-btn" data-for="${name}"></button>`;

/* Phan "giong noi" dung chung cho tac nhan, goi cuoc, cai dat.
   ctx.defaults: gia tri ap dung khi de trong {stt_model, tts_provider, tts_model, tts_voice}
   ctx.defaultLabel: chu hien canh gia tri mac dinh ("Theo goi cuoc", "Mac dinh he thong"...)
   ctx.lang(): ngon ngu hien tai (de goi y giong Edge). */
function voiceFieldsHtml(v, ctx) {
  const prov = (p) => p === 'edge' ? t('provider_edge') : t('provider_openrouter');
  return `
    <label class="f"><span>${t('stt_model')}</span>${modelInput('stt_model', v.stt_model)}</label>
    <label class="f"><span>${t('tts_provider')}</span><select name="tts_provider">
      ${ctx.allowEmpty ? `<option value="">${esc(ctx.defaultLabel)} (${prov(ctx.defaults.tts_provider)})</option>` : ''}
      <option value="edge" ${v.tts_provider === 'edge' ? 'selected' : ''}>${t('provider_edge')}</option>
      <option value="openrouter" ${v.tts_provider === 'openrouter' ? 'selected' : ''}>${t('provider_openrouter')}</option></select></label>
    <label class="f"><span>${t('tts_model')}</span>${modelInput('tts_model', v.tts_model)}</label>
    <label class="f"><span>${t('tts_voice')}</span><input type="hidden" name="tts_voice" value="${esc(v.tts_voice || '')}"><button type="button" class="model-btn" data-voice></button></label>`;
}

function bindVoiceFields(form, ctx) {
  const effProvider = () => form.tts_provider.value || ctx.defaults.tts_provider;
  const sttBtn = form.querySelector('[data-for="stt_model"]');
  const ttsBtn = form.querySelector('[data-for="tts_model"]');
  const voiceBtn = form.querySelector('[data-voice]');
  const fb = (k) => ({ fallbackId: ctx.defaults[k], fallbackLabel: ctx.defaultLabel });
  const voicesFor = async () => {
    if (effProvider() === 'edge') {
      const v = await api('/voices/edge');
      return [...(v[ctx.lang()] || []), ...Object.values(v).flat().filter((x) => !(v[ctx.lang()] || []).includes(x))];
    }
    const id = form.tts_model.value || ctx.defaults.tts_model;
    const m = (await catalog('tts')).find((x) => x.id === id);
    return (m && m.voices) || [];
  };
  const render = async () => {
    sttBtn.innerHTML = await modelBtnHtml({ kind: 'stt', id: form.stt_model.value, ...fb('stt_model') });
    const edge = effProvider() === 'edge';
    ttsBtn.disabled = edge;
    ttsBtn.innerHTML = edge
      ? `<div class="nm"><b>Edge TTS</b><small>${t('free')} · ${t('edge_note')}</small></div>`
      : await modelBtnHtml({ kind: 'tts', id: form.tts_model.value, ...(ctx.allowEmpty || !form.tts_model.value ? { fallbackId: ctx.defaults.tts_model, fallbackLabel: ctx.defaultLabel } : {}) });
    const voice = form.tts_voice.value || (ctx.allowEmpty ? ctx.defaults.tts_voice : '');
    voiceBtn.innerHTML = `<div class="nm"><b>${esc(voice || t('voice_auto'))}</b><small>${voice ? (form.tts_voice.value ? '' : esc(ctx.defaultLabel)) : t('voice_auto_hint')}</small></div>${icon('edit')}`;
  };
  sttBtn.onclick = async () => {
    const id = await pickModel({ kind: 'stt', current: form.stt_model.value });
    if (id) { form.stt_model.value = id; render(); }
  };
  ttsBtn.onclick = async () => {
    const id = await pickModel({ kind: 'tts', current: form.tts_model.value });
    if (id) { form.tts_model.value = id; form.tts_voice.value = ''; render(); }
  };
  voiceBtn.onclick = async () => {
    const v = await pickVoice(await voicesFor(), form.tts_voice.value);
    if (v !== null) { form.tts_voice.value = v; render(); }
  };
  form.tts_provider.addEventListener('change', () => { form.tts_voice.value = ''; render(); });
  render();
  return render;
}

const cardTitle = (ic, title, sub = '') =>
  `<div class="card-title"><span class="avatar sm">${icon(ic)}</span><div><h2>${title}</h2>${sub ? `<p>${sub}</p>` : ''}</div></div>`;

// ---------- khoa API + goi cuoc cua toi ----------

function planCard(plan) {
  if (!plan) {
    return `<div class="card plan-card none">${cardTitle('card', t('my_plan'), t('no_plan_sub'))}</div>`;
  }
  return `<div class="card plan-card">
    <div class="card-title"><span class="avatar sm">${icon('card')}</span><div><h2>${t('my_plan')}: ${esc(plan.name)}</h2>
      <p>${esc(plan.description || '')}</p></div>${plan.price_label ? `<span class="pill plain" style="margin-left:auto">${esc(plan.price_label)}</span>` : ''}</div>
    <div class="kv" style="margin:0"><span>${t('llm_model')}</span><span>${esc(plan.llm_model || '–')}</span>
      <span>${t('expires')}</span><span>${plan.expires_at ? new Date(plan.expires_at * 1000).toLocaleDateString(lang === 'vi' ? 'vi-VN' : 'en-US') : t('no_expiry')}</span></div>
  </div>`;
}

async function keysPage() {
  const info = await api('/providers');
  shell('keys', `
    ${pageHead(t('keys'), t('keys_sub'))}
    <div class="stack">
      ${planCard(info.plan)}
      <div class="grid">${info.providers.map((p) => `
        <form class="card item key-card" data-p="${p.id}">
          <div class="item-head"><span class="avatar ${p.key_hint ? '' : 'muted'}">${icon('key')}</span>
            <div class="t"><b>${esc(p.name)}</b><small>${p.key_hint ? esc(p.key_hint) : t('not_connected')}</small></div>
            ${p.key_hint ? `<span class="pill ok">${t('connected')}</span>` : ''}</div>
          <div class="key-row"><input type="password" name="api_key" placeholder="${t('paste_key')}" autocomplete="off" required minlength="8">
            <button class="btn sm">${t('save')}</button></div>
          <div class="item-foot" style="border:0;padding-top:12px">
            <a href="${p.keys_url}" target="_blank" rel="noopener">${t('get_key')} ↗</a>
            ${p.key_hint ? `<button type="button" class="btn danger sm" data-rm="${p.id}" style="margin-left:auto">${icon('trash')}${t('remove')}</button>` : ''}
          </div>
        </form>`).join('')}</div>
    </div>`);
  app.querySelectorAll('form[data-p]').forEach((f) => f.onsubmit = async (e) => {
    e.preventDefault();
    const btn = f.querySelector('button.btn'); btn.disabled = true;
    try {
      await api(`/keys/${f.dataset.p}`, { method: 'PUT', body: { api_key: f.api_key.value.trim() } });
      clearCatalog(f.dataset.p); toast('key_saved'); keysPage();
    } catch (err) { toast(err.message.startsWith('invalid_key') ? 'invalid_key' : err.message, true); btn.disabled = false; }
  });
  app.querySelectorAll('[data-rm]').forEach((b) => b.onclick = async () => {
    await api(`/keys/${b.dataset.rm}`, { method: 'DELETE' }); clearCatalog(b.dataset.rm); toast('key_removed'); keysPage();
  });
}

// ---------- tac nhan ----------

async function agentsPage() {
  const agents = await api('/agents');
  const newBtn = `<a class="btn" href="#/agents/new">${icon('plus')}${t('new_agent')}</a>`;
  shell('agents', `
    ${pageHead(t('agents'), t('agents_sub'), newBtn)}
    ${agents.length ? `<div class="grid">${agents.map((a) => `
      <div class="card item">
        <div class="item-head">
          <span class="avatar">${initial(a.name)}</span>
          <div class="t"><b>${esc(a.name)}</b><small>${esc(a.llm_provider ? `${a.llm_model} · ${t('own_key')}` : t('by_plan'))}</small></div>
          <span class="pill plain">${a.language.toUpperCase()}</span>
        </div>
        <p class="clamp grow">${esc(a.system_prompt) || '–'}</p>
        <div class="item-foot">
          <span class="muted" style="align-self:center">${icon('device').replace('class="i"', 'class="i" style="vertical-align:-4px"')} ${a.device_count} ${t('devices_count')}</span>
          <a class="btn ghost sm" href="#/agents/${a.id}" style="margin-left:auto">${icon('edit')}${t('edit')}</a>
          <button class="btn danger sm" data-del="${a.id}">${icon('trash')}</button>
        </div>
      </div>`).join('')}</div>`
    : emptyState('agent', t('no_agents'), t('no_agents_sub'), newBtn)}`);
  app.querySelectorAll('[data-del]').forEach((b) => b.onclick = async () => {
    if (!confirm(t('confirm_delete_agent'))) return;
    await api(`/agents/${b.dataset.del}`, { method: 'DELETE' }); agentsPage();
  });
}

async function agentEditPage(id) {
  const isNew = id === 'new';
  const [a, prov] = await Promise.all([
    isNew ? { name: '', language: lang, system_prompt: PROMPT_TEMPLATES[lang][0][1], llm_provider: '', llm_model: '', stt_model: '',
      tts_provider: '', tts_model: '', tts_voice: '', history_turns: 6 } : api(`/agents/${id}`),
    api('/providers')]);
  // "Theo goi cuoc" (hoac mac dinh he thong voi admin chua co goi), hay key rieng cua nguoi dung.
  const baseLabel = prov.plan ? `${t('by_plan')}: ${prov.plan.name}` : prov.is_admin ? t('system_default') : t('by_plan');
  const usable = (p) => !!p.key_hint || (p.id === 'openrouter' && prov.is_admin && prov.system_key);
  const provOptions = `<option value="">${esc(baseLabel)}${prov.plan || prov.is_admin ? ` — ${esc(prov.default_model)}` : ` — ${t('no_plan_short')}`}</option>` +
    `<optgroup label="${t('own_key')}">` + prov.providers.map((p) =>
      `<option value="${p.id}" ${a.llm_provider === p.id ? 'selected' : ''} ${usable(p) || a.llm_provider === p.id ? '' : 'disabled'}>${esc(p.name)}${p.key_hint ? ' ✓' : usable(p) ? ` (${t('uses_system_key')})` : ` (${t('needs_key')})`}</option>`).join('') + '</optgroup>';
  const field = (label, inner, help = '') => `<label class="f"><span>${label}</span>${inner}${help ? `<small>${help}</small>` : ''}</label>`;
  shell('agents', `
    ${pageHead(isNew ? t('new_agent') : esc(a.name), t('agents_sub'), '', crumbLink('#/agents'))}
    <form id="f" class="stack">
      <div class="card">
        ${cardTitle('agent', t('persona_section'))}
        <div class="two">
          ${field(t('name'), `<input name="name" value="${esc(a.name)}" required maxlength="60">`)}
          ${field(t('language'), `<select name="language">
            <option value="vi" ${a.language === 'vi' ? 'selected' : ''}>${t('lang_vi')}</option>
            <option value="en" ${a.language === 'en' ? 'selected' : ''}>${t('lang_en')}</option></select>`)}
        </div>
        <label class="f"><span>${t('system_prompt')}</span>
          <div class="chips" id="tpl"></div>
          <textarea name="system_prompt" maxlength="4000">${esc(a.system_prompt)}</textarea></label>
        ${field(t('history_turns'), `<input type="number" name="history_turns" min="0" max="30" value="${a.history_turns}" style="max-width:160px">`)}
      </div>
      <div class="card">
        ${cardTitle('cpu', t('llm_section'), t('llm_section_sub'))}
        ${!prov.plan && !prov.is_admin ? `<div class="notice" style="margin-bottom:16px">${icon('alert')}<span>${t('no_plan_sub')}</span></div>` : ''}
        <div class="two">
          ${field(t('llm_provider'), `<select name="llm_provider">${provOptions}</select>`, `<span id="prov-hint"></span>`)}
          ${field(t('llm_model'), modelInput('llm_model', a.llm_model))}
        </div>
      </div>
      <div class="card">
        ${cardTitle('mic', t('voice_section'), t('voice_section_sub'))}
        <div class="two">${voiceFieldsHtml(a, { allowEmpty: true, defaultLabel: baseLabel, defaults: prov.voice_defaults })}</div>
      </div>
      <div class="row-actions"><button class="btn">${t('save')}</button><a class="btn ghost" href="#/agents">${t('cancel')}</a></div>
    </form>`);

  const form = document.getElementById('f');
  const renderTemplates = () => {
    const l = form.language.value;
    const box = document.getElementById('tpl');
    box.innerHTML = `<span>${t('templates')}</span>` + PROMPT_TEMPLATES[l].map((p, i) => `<button type="button" data-i="${i}">${esc(p[0])}</button>`).join('');
    box.querySelectorAll('button').forEach((b) => b.onclick = () => { form.system_prompt.value = PROMPT_TEMPLATES[l][b.dataset.i][1]; });
  };
  renderTemplates();
  const renderVoice = bindVoiceFields(form, { allowEmpty: true, defaultLabel: baseLabel, defaults: prov.voice_defaults, lang: () => form.language.value });
  form.language.onchange = () => { renderTemplates(); renderVoice(); };

  const llmBtn = form.querySelector('[data-for="llm_model"]');
  const renderLlm = async () => {
    const p = form.llm_provider.value;
    llmBtn.disabled = !p;
    llmBtn.innerHTML = p ? await modelBtnHtml({ kind: 'llm', provider: p, id: form.llm_model.value })
      : await modelBtnHtml({ kind: 'llm', id: '', fallbackId: prov.default_model, fallbackLabel: baseLabel });
    const info = prov.providers.find((x) => x.id === p);
    document.getElementById('prov-hint').innerHTML = info && !usable(info) ? `<a href="#/keys">${t('provider_key_hint')}</a>` : '';
  };
  form.llm_provider.onchange = () => { form.llm_model.value = ''; renderLlm(); if (form.llm_provider.value) llmBtn.click(); };
  llmBtn.onclick = async () => {
    const chosen = await pickModel({ kind: 'llm', provider: form.llm_provider.value, current: form.llm_model.value });
    if (chosen) { form.llm_model.value = chosen; renderLlm(); }
  };
  renderLlm();

  form.onsubmit = async (e) => {
    e.preventDefault();
    const d = formData(form);
    d.history_turns = +d.history_turns;
    if (d.llm_provider && !d.llm_model) { toast('choose_model', true); return; }
    try {
      const saved = await api(isNew ? '/agents' : `/agents/${id}`, { method: isNew ? 'POST' : 'PUT', body: d });
      toast('saved');
      if (isNew) location.hash = `#/agents/${saved.id}`;
    } catch (err) { toast(err.message, true); }
  };
}

// ---------- goi cuoc (admin) ----------

async function plansPage() {
  const plans = await api('/plans');
  const newBtn = `<a class="btn" href="#/plans/new">${icon('plus')}${t('new_plan')}</a>`;
  const provName = { openrouter: 'OpenRouter', openai: 'OpenAI', gemini: 'Gemini', deepseek: 'DeepSeek', groq: 'Groq' };
  shell('plans', `
    ${pageHead(t('plans'), t('plans_sub'), newBtn)}
    ${plans.length ? `<div class="grid">${plans.map((p) => `
      <div class="card item">
        <div class="item-head"><span class="avatar">${icon('card')}</span>
          <div class="t"><b>${esc(p.name)}</b><small>${esc(provName[p.llm_provider] || p.llm_provider)} · ${esc(p.llm_model || t('system_default'))}</small></div>
          ${p.price_label ? `<span class="pill plain">${esc(p.price_label)}</span>` : ''}</div>
        <p class="clamp grow">${esc(p.description) || '–'}</p>
        <div class="kv"><span>${t('api_key')}</span><span>${p.api_key_hint ? esc(p.api_key_hint) : t('uses_system_key')}</span>
          <span>${t('users')}</span><span>${p.user_count}</span></div>
        <div class="item-foot">
          <a class="btn ghost sm" href="#/plans/${p.id}" style="margin-left:auto">${icon('edit')}${t('edit')}</a>
          <button class="btn danger sm" data-del="${p.id}">${icon('trash')}</button>
        </div>
      </div>`).join('')}</div>`
    : emptyState('card', t('no_plans'), t('no_plans_sub'), newBtn)}`);
  app.querySelectorAll('[data-del]').forEach((b) => b.onclick = async () => {
    if (!confirm(t('confirm_delete_plan'))) return;
    await api(`/plans/${b.dataset.del}`, { method: 'DELETE' }); plansPage();
  });
}

async function planEditPage(id) {
  const isNew = id === 'new';
  const [p, prov, s] = await Promise.all([
    isNew ? { name: '', description: '', price_label: '', llm_provider: 'openrouter', llm_model: '', api_key_hint: '',
      stt_model: '', tts_provider: '', tts_model: '', tts_voice: '' } : api(`/plans/${id}`),
    api('/providers'), api('/settings')]);
  const D = s.defaults;
  const sysDefaults = { stt_model: s.default_stt_model || D.stt_model, tts_provider: s.default_tts_provider || D.tts_provider,
    tts_model: s.default_tts_model || D.tts_model, tts_voice: s.default_tts_voice || '' };
  const field = (label, inner, help = '') => `<label class="f"><span>${label}</span>${inner}${help ? `<small>${help}</small>` : ''}</label>`;
  shell('plans', `
    ${pageHead(isNew ? t('new_plan') : esc(p.name), t('plans_sub'), '', crumbLink('#/plans'))}
    <form id="f" class="stack">
      <div class="card">
        ${cardTitle('card', t('plan_info'))}
        <div class="two">
          ${field(t('name'), `<input name="name" value="${esc(p.name)}" required maxlength="60" placeholder="Basic, Pro…">`)}
          ${field(t('price_label'), `<input name="price_label" value="${esc(p.price_label)}" maxlength="60" placeholder="99.000đ / tháng">`)}
        </div>
        ${field(t('description'), `<textarea name="description" maxlength="500" style="min-height:80px">${esc(p.description)}</textarea>`)}
      </div>
      <div class="card">
        ${cardTitle('cpu', t('llm_section'), t('plan_llm_sub'))}
        <div class="two">
          ${field(t('llm_provider'), `<select name="llm_provider">${prov.providers.map((x) =>
            `<option value="${x.id}" ${p.llm_provider === x.id ? 'selected' : ''}>${esc(x.name)}</option>`).join('')}</select>`)}
          ${field(t('llm_model'), modelInput('llm_model', p.llm_model))}
        </div>
        ${field(`${t('plan_api_key')} ${p.api_key_hint ? `<span class="pill ok" style="margin-left:6px">${esc(p.api_key_hint)}</span>` : ''}`,
          `<div class="key-row"><input type="password" name="api_key" placeholder="${p.api_key_hint ? t('keep_key') : t('paste_key')}" autocomplete="off">
           ${p.api_key_hint ? `<button type="button" class="btn danger sm" id="clear-key">${t('remove')}</button>` : ''}</div>`, `<span id="key-help"></span>`)}
      </div>
      <div class="card">
        ${cardTitle('mic', t('voice_section'), t('plan_voice_sub'))}
        <div class="two">${voiceFieldsHtml(p, { allowEmpty: true, defaultLabel: t('system_default'), defaults: sysDefaults })}</div>
      </div>
      <div class="row-actions"><button class="btn">${t('save')}</button><a class="btn ghost" href="#/plans">${t('cancel')}</a></div>
    </form>`);
  const form = document.getElementById('f');
  let clearKey = false;
  bindVoiceFields(form, { allowEmpty: true, defaultLabel: t('system_default'), defaults: sysDefaults, lang: () => lang });
  const llmBtn = form.querySelector('[data-for="llm_model"]');
  const renderLlm = async () => {
    const pr = form.llm_provider.value;
    llmBtn.innerHTML = await modelBtnHtml({ kind: 'llm', provider: pr, planId: isNew ? null : id, id: form.llm_model.value,
      ...(pr === 'openrouter' ? { fallbackId: s.default_llm_model || D.llm_model, fallbackLabel: t('system_default') } : {}) });
    document.getElementById('key-help').textContent = pr === 'openrouter' ? t('plan_key_optional') : t('plan_key_required');
  };
  form.llm_provider.onchange = () => { form.llm_model.value = ''; renderLlm(); };
  llmBtn.onclick = async () => {
    const chosen = await pickModel({ kind: 'llm', provider: form.llm_provider.value, planId: isNew ? null : id, current: form.llm_model.value });
    if (chosen) { form.llm_model.value = chosen; renderLlm(); }
  };
  renderLlm();
  document.getElementById('clear-key')?.addEventListener('click', (e) => {
    clearKey = true; e.target.closest('.f').querySelector('.pill')?.remove(); e.target.remove();
  });
  form.onsubmit = async (e) => {
    e.preventDefault();
    const d = formData(form);
    d.api_key = d.api_key.trim() ? d.api_key.trim() : clearKey ? '' : null;
    try {
      const saved = await api(isNew ? '/plans' : `/plans/${id}`, { method: isNew ? 'POST' : 'PUT', body: d });
      clearCatalog(d.llm_provider); toast('saved');
      if (isNew) location.hash = `#/plans/${saved.id}`; else planEditPage(id);
    } catch (err) { toast(err.message, true); }
  };
}

// ---------- cai dat (admin) ----------

async function settingsPage() {
  const [s, info] = await Promise.all([api('/settings'), api('/settings/key-info')]);
  const D = s.defaults;
  const v = { stt_model: s.default_stt_model, tts_provider: s.default_tts_provider || D.tts_provider,
    tts_model: s.default_tts_model, tts_voice: s.default_tts_voice };
  shell('settings', `
    ${pageHead(t('settings'), t('settings_sub'))}
    <form id="f" class="stack">
      ${!s.openrouter_api_key_set ? `<div class="notice">${icon('alert')}<span>${t('api_key_missing')}</span></div>` : ''}
      <div class="card">
        ${cardTitle('key', t('api_section'), t('api_key_help'))}
        <label class="f"><span>${t('api_key')} ${s.openrouter_api_key_set ? `<span class="pill ok" style="margin-left:6px">${t('api_key_set')} · ${esc(s.openrouter_api_key_hint)}</span>` : ''}</span>
          <input type="password" name="openrouter_api_key" placeholder="sk-or-v1-…" autocomplete="off"></label>
        ${info.set && !info.error ? `<div class="kv" style="margin:0"><span>${t('credits')}</span><span>$${(+info.usage || 0).toFixed(4)}</span>
          <span>${t('limit')}</span><span>${info.limit == null ? '∞' : '$' + info.limit}</span></div>` : ''}
        ${info.error ? `<div class="notice err">${icon('alert')}<span>${esc(info.error)}</span></div>` : ''}
      </div>
      <div class="card">
        ${cardTitle('cpu', t('defaults'), t('defaults_sub'))}
        <div class="two">
          <label class="f"><span>${t('llm_model')}</span>${modelInput('default_llm_model', s.default_llm_model)}</label>
          ${voiceFieldsHtml(v, { allowEmpty: false, defaults: D })}
        </div>
      </div>
      <div class="card">
        ${cardTitle('users', t('users'))}
        <label class="switch"><input type="checkbox" name="allow_registration" ${s.allow_registration ? 'checked' : ''}>${t('allow_registration')}</label>
      </div>
      <div class="row-actions"><button class="btn">${t('save')}</button></div>
    </form>`);
  const f = document.getElementById('f');
  bindVoiceFields(f, { allowEmpty: false, defaultLabel: t('builtin_default'), defaults: { ...D, tts_voice: '' }, lang: () => lang });
  const llmBtn = f.querySelector('[data-for="default_llm_model"]');
  const renderLlm = async () => { llmBtn.innerHTML = await modelBtnHtml({ kind: 'llm', id: f.default_llm_model.value, fallbackId: D.llm_model, fallbackLabel: t('builtin_default') }); };
  llmBtn.onclick = async () => {
    const chosen = await pickModel({ kind: 'llm', current: f.default_llm_model.value || D.llm_model });
    if (chosen) { f.default_llm_model.value = chosen; renderLlm(); }
  };
  renderLlm();
  f.onsubmit = async (e) => {
    e.preventDefault();
    const d = formData(f);
    if (!d.openrouter_api_key) delete d.openrouter_api_key;
    d.allow_registration = f.allow_registration.checked;
    // Giao dien dung ten truong chung; API cai dat dung tien to default_.
    for (const k of ['stt_model', 'tts_provider', 'tts_model', 'tts_voice']) { d['default_' + k] = d[k]; delete d[k]; }
    try { await api('/settings', { method: 'PUT', body: d }); toast('saved'); settingsPage(); }
    catch (err) { toast(err.message, true); }
  };
}

// ---------- nguoi dung (admin) ----------

async function usersPage() {
  const [users, plans] = await Promise.all([api('/users'), api('/plans')]);
  const toDate = (ts) => ts ? new Date(ts * 1000).toISOString().slice(0, 10) : '';
  const expired = (u) => u.plan_expires_at && u.plan_expires_at * 1000 < Date.now();
  shell('users', `
    ${pageHead(t('users'), t('users_sub'), `<button class="btn" id="add">${icon('plus')}${t('add_user')}</button>`)}
    <div class="card table-wrap"><table><thead><tr><th>${t('username')}</th><th>${t('role')}</th><th>${t('plan')}</th><th>${t('expires')}</th><th>${t('devices')}</th><th></th></tr></thead><tbody>
      ${users.map((u) => `<tr data-u="${u.id}">
        <td><div class="who"><span class="avatar sm">${initial(u.username)}</span>${esc(u.username)}</div></td>
        <td><select data-k="role" ${u.id === me.id ? 'disabled' : ''}><option value="user" ${u.role === 'user' ? 'selected' : ''}>${t('role_user')}</option><option value="admin" ${u.role === 'admin' ? 'selected' : ''}>${t('role_admin')}</option></select></td>
        <td><select data-k="plan_id"><option value="0">${t('no_plan_short')}</option>${plans.map((p) =>
          `<option value="${p.id}" ${u.plan_id === p.id ? 'selected' : ''}>${esc(p.name)}</option>`).join('')}</select></td>
        <td><input type="date" data-k="plan_expires_at" value="${toDate(u.plan_expires_at)}" title="${t('no_expiry')}">${expired(u) ? `<div class="tag" style="color:var(--err);margin-top:4px">${t('expired')}</div>` : ''}</td>
        <td>${u.device_count}</td>
        <td style="text-align:right">${u.id !== me.id ? `<button class="btn danger sm" data-del="${u.id}">${icon('trash')}</button>` : ''}</td></tr>`).join('')}
    </tbody></table></div>
    ${plans.length ? '' : `<p class="muted" style="margin-top:12px"><a href="#/plans">${t('no_plans_sub')}</a></p>`}`);
  app.querySelectorAll('tr[data-u] [data-k]').forEach((el) => el.onchange = async () => {
    const uid = el.closest('tr').dataset.u;
    let value = el.value;
    if (el.dataset.k === 'plan_id') value = +value;
    if (el.dataset.k === 'plan_expires_at') value = value ? new Date(value + 'T23:59:59').getTime() / 1000 : 0;
    try { await api(`/users/${uid}`, { method: 'PUT', body: { [el.dataset.k]: value } }); toast('saved'); usersPage(); }
    catch (err) { toast(err.message, true); }
  });
  document.getElementById('add').onclick = () => modal(`<h2>${t('add_user')}</h2><p></p>
    <label class="f"><span>${t('username')}</span><input name="username" required minlength="3"></label>
    <label class="f"><span>${t('password')}</span><input type="password" name="password" required minlength="8"></label>
    <label class="f"><span>${t('role')}</span><select name="role"><option value="user">${t('role_user')}</option><option value="admin">${t('role_admin')}</option></select></label>
    ${modalActions(t('create'))}`,
    async (d) => { await api('/users', { body: d }); usersPage(); });
  app.querySelectorAll('[data-del]').forEach((b) => b.onclick = async () => {
    if (!confirm(t('confirm_delete_user'))) return;
    await api(`/users/${b.dataset.del}`, { method: 'DELETE' }); usersPage();
  });
}

// ---------- router ----------

let refreshTimer = null;
async function route() {
  clearInterval(refreshTimer);
  document.documentElement.lang = lang;
  if (!me) {
    const st = await api('/auth/state');
    authInfo = st;
    if (!st.user) return authScreen(st.needs_setup ? 'setup' : 'login');
    me = st.user;
  }
  const [, page, id] = (location.hash || '#/devices').split('/');
  try {
    if (page === 'agents' && id) await agentEditPage(id);
    else if (page === 'agents') await agentsPage();
    else if (page === 'devices' && id) await deviceHistoryPage(id);
    else if (page === 'keys') await keysPage();
    else if (page === 'settings' && me.role === 'admin') await settingsPage();
    else if (page === 'plans' && me.role === 'admin' && id) await planEditPage(id);
    else if (page === 'plans' && me.role === 'admin') await plansPage();
    else if (page === 'users' && me.role === 'admin') await usersPage();
    else {
      await devicesPage();
      // Cap nhat trang thai online moi 5 giay (khi khong co hop thoai dang mo).
      refreshTimer = setInterval(() => { if (!document.querySelector('.backdrop') && document.activeElement.tagName !== 'SELECT') devicesPage(); }, 5000);
    }
  } catch (err) {
    if (err.message !== '401') toast(err.message, true);
  }
}

window.addEventListener('hashchange', route);
route();
