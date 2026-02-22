const log = document.getElementById('log');
const tokenInput = document.getElementById('token');
const sessionInput = document.getElementById('session');
const messageInput = document.getElementById('message');
const sendBtn = document.getElementById('send');
const sessionsEl = document.getElementById('sessions');
const approvalsEl = document.getElementById('approvals');
const routeEl = document.getElementById('route');
const traceEl = document.getElementById('trace');
const sessionMetaEl = document.getElementById('session-meta');
const copyExportBtn = document.getElementById('copy-export');
const copySessionBtn = document.getElementById('copy-session');
const clearLogBtn = document.getElementById('clear-log');
const logFilter = document.getElementById('log-filter');
const canvasView = document.getElementById('canvas-view');
const canvasEditor = document.getElementById('canvas-editor');
const canvasSave = document.getElementById('canvas-save');
const canvasRefresh = document.getElementById('canvas-refresh');
const canvasValidate = document.getElementById('canvas-validate');
const canvasRender = document.getElementById('canvas-render');
const canvasStatus = document.getElementById('canvas-status');
const refreshSessionsBtn = document.getElementById('refresh-sessions');
const refreshApprovalsBtn = document.getElementById('refresh-approvals');
const connectionStatus = document.getElementById('connection-status');
const statusText = connectionStatus.querySelector('.status-text');

const MAX_LOG_ITEMS = 300;
const traceState = new Map();
let logFilterValue = 'all';
let ws = null;

const params = new URLSearchParams(window.location.search);
const allowAnon = params.get('anon') === '1';
const e2eMode = params.get('e2e') === '1';
if (params.get('token')) {
  tokenInput.value = params.get('token');
}

function setConnectionState(connected, label) {
  connectionStatus.classList.toggle('connected', connected);
  statusText.textContent = label || (connected ? 'Connected' : 'Disconnected');
}

function hasToken() {
  return tokenInput.value.trim().length > 0;
}

function canUseAnonymous() {
  return allowAnon;
}

function wsUrl() {
  const token = tokenInput.value.trim();
  const scheme = window.location.protocol === 'https:' ? 'wss' : 'ws';
  if (!token) return `${scheme}://${location.host}/`;
  return `${scheme}://${location.host}/?token=${encodeURIComponent(token)}`;
}

function authHeaders(extra) {
  const token = tokenInput.value.trim();
  const headers = extra ? { ...extra } : {};
  if (token) {
    headers['Authorization'] = `Bearer ${token}`;
  }
  return headers;
}

function connectSocket() {
  if (ws) {
    ws.close();
  }
  if (!hasToken() && !canUseAnonymous()) {
    setConnectionState(false, 'Token required');
    return;
  }
  if (!hasToken() && canUseAnonymous()) {
    setConnectionState(false, 'Anonymous');
  }
  ws = new WebSocket(wsUrl());
  if (hasToken()) {
    setConnectionState(false, 'Connecting');
  }

  ws.onopen = () => {
    setConnectionState(true, 'Connected');
    sendConnect();
  };

  ws.onclose = () => {
    setConnectionState(false, 'Disconnected');
    renderLogItem('event', 'WebSocket disconnected', '', 'socket');
  };

  ws.onmessage = (evt) => {
    const parsed = safeJson(evt.data);
    if (!parsed) {
      renderLogItem('raw', evt.data, '', 'raw');
      return;
    }
    if (parsed.type === 'event' && parsed.event === 'agent') {
      const payload = parsed.data || {};
      const runId = payload.run_id || 'unknown';
      traceState.set(runId, {
        run_id: runId,
        agent_id: payload.agent_id || 'agent',
        status: payload.status || 'stream',
        ts: parsed.ts || '',
      });
      renderTrace();
    }
    const formatted = formatEvent(parsed);
    renderLogItem(formatted.kind, formatted.title, formatted.body, formatted.meta);
  };
}

function sendConnect() {
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  const payload = {
    type: 'request',
    id: `req_connect_${Date.now()}`,
    method: 'gateway.connect',
    params: { protocol: 'v0.1', client_id: 'webchat' },
  };
  ws.send(JSON.stringify(payload));
}

function safeJson(text) {
  try {
    return JSON.parse(text);
  } catch (err) {
    return null;
  }
}

function shouldRender(kind) {
  if (logFilterValue === 'all') return true;
  if (logFilterValue === 'agent') return kind === 'agent';
  if (logFilterValue === 'event') return kind === 'event' || kind === 'presence' || kind === 'tick';
  if (logFilterValue === 'error') return kind === 'error';
  return true;
}

function renderLogItem(kind, title, body, meta) {
  if (!shouldRender(kind)) return;
  const wrapper = document.createElement('div');
  wrapper.className = 'log-item';
  const metaEl = document.createElement('div');
  metaEl.className = 'log-meta';
  metaEl.textContent = meta || kind;
  const bodyEl = document.createElement('div');
  bodyEl.className = 'log-body';
  bodyEl.textContent = title || '';
  if (body) {
    const bodyDetail = document.createElement('div');
    bodyDetail.className = 'log-code';
    bodyDetail.textContent = body;
    bodyEl.appendChild(bodyDetail);
  }
  wrapper.appendChild(metaEl);
  wrapper.appendChild(bodyEl);
  log.appendChild(wrapper);
  while (log.children.length > MAX_LOG_ITEMS) {
    log.removeChild(log.firstChild);
  }
  log.scrollTop = log.scrollHeight;
}

function formatEvent(data) {
  if (!data || typeof data !== 'object') {
    return { kind: 'raw', title: String(data || ''), body: '' };
  }
  if (data.type === 'event') {
    const evt = data.event || 'event';
    const seq = data.seq ? `#${data.seq}` : '';
    if (evt === 'agent') {
      const payload = data.data || {};
      const agent = payload.agent_id || 'agent';
      const status = payload.status || 'update';
      const content = payload.content || '';
      return {
        kind: 'agent',
        title: `${agent} - ${status}`,
        body: content,
        meta: `agent ${seq}`.trim(),
      };
    }
    return {
      kind: evt === 'agent' ? 'agent' : 'event',
      title: `${evt} ${seq}`.trim(),
      body: JSON.stringify(data.data || {}, null, 2),
      meta: evt,
    };
  }
  if (data.type === 'response') {
    if (data.ok) {
      return {
        kind: 'response',
        title: 'Request acknowledged',
        body: JSON.stringify(data.result || {}, null, 2),
        meta: `response ${data.id || ''}`.trim(),
      };
    }
    return {
      kind: 'error',
      title: (data.error && data.error.message) || 'Request failed',
      body: JSON.stringify(data.error || {}, null, 2),
      meta: `error ${data.id || ''}`.trim(),
    };
  }
  if (data.type === 'request') {
    return {
      kind: 'request',
      title: `${data.method || 'request'}`,
      body: JSON.stringify(data.params || {}, null, 2),
      meta: `request ${data.id || ''}`.trim(),
    };
  }
  return { kind: 'raw', title: JSON.stringify(data), body: '' };
}

async function refreshSessions() {
  try {
    const res = await fetch('/sessions', { headers: authHeaders() });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    const fragment = document.createDocumentFragment();
    (data.sessions || []).forEach((s) => {
      const btn = document.createElement('button');
      btn.className = 'item';
      btn.type = 'button';
      btn.textContent = `${s.session_id} (${s.message_count})`;
      btn.onclick = () => {
        sessionInput.value = s.session_id;
        refreshSessions();
        refreshCanvas();
      };
      fragment.appendChild(btn);
    });
    sessionsEl.replaceChildren(fragment);

    const current = (data.sessions || []).find((s) => s.session_id === sessionInput.value);
    if (current && current.meta) {
      const owner = current.meta.owner || 'unknown';
      const channel = current.meta.channel_id || 'webchat';
      const scope = current.meta.scope || 'dm';
      routeEl.textContent = `${owner} via ${channel} (${scope})`;
      renderSessionMeta(current.meta, current.message_count || 0);
    } else {
      routeEl.textContent = 'None';
      sessionMetaEl.textContent = 'No session selected.';
    }
  } catch (err) {
    renderLogItem('error', 'Sessions refresh failed', String(err), 'http');
  }
}

async function refreshApprovals() {
  try {
    const res = await fetch('/approvals', { headers: authHeaders() });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    const fragment = document.createDocumentFragment();
    (data.pending || []).forEach((a) => {
      const entry = document.createElement('div');
      entry.className = 'item';
      const content = (a.content || '').trim();
      const snippet = content.length > 80 ? `${content.slice(0, 80)}...` : content;
      const context = snippet || 'Execution approval requested';
      const title = document.createElement('div');
      title.textContent = `${a.agent_id}: ${context}`;
      entry.appendChild(title);
      if (a.requested_at) {
        const stamp = document.createElement('small');
        stamp.textContent = a.requested_at;
        entry.appendChild(stamp);
      }
      const actions = document.createElement('div');
      actions.className = 'controls';
      const approve = document.createElement('button');
      approve.textContent = 'Approve';
      approve.className = 'primary';
      approve.onclick = async () => {
        await fetch(`/approvals/${a.run_id}/approve`, { method: 'POST', headers: authHeaders() });
        refreshApprovals();
      };
      const reject = document.createElement('button');
      reject.textContent = 'Reject';
      reject.className = 'ghost';
      reject.onclick = async () => {
        await fetch(`/approvals/${a.run_id}/reject`, { method: 'POST', headers: authHeaders() });
        refreshApprovals();
      };
      actions.appendChild(approve);
      actions.appendChild(reject);
      entry.appendChild(actions);
      fragment.appendChild(entry);
    });
    approvalsEl.replaceChildren(fragment);
  } catch (err) {
    renderLogItem('error', 'Approvals refresh failed', String(err), 'http');
  }
}

function renderCanvas(state) {
  const fragment = document.createDocumentFragment();
  const widgets = (state && state.widgets) || [];
  widgets.forEach((widget) => {
    if (widget.type === 'card') {
      const card = document.createElement('div');
      card.className = 'widget';
      const title = document.createElement('div');
      title.className = 'widget-title';
      title.textContent = widget.title || 'Card';
      const body = document.createElement('div');
      body.textContent = widget.body || '';
      card.appendChild(title);
      card.appendChild(body);
      fragment.appendChild(card);
    } else if (widget.type === 'metric') {
      const row = document.createElement('div');
      row.className = 'widget';
      const label = document.createElement('div');
      label.className = 'widget-title';
      label.textContent = widget.label || 'Metric';
      const value = document.createElement('div');
      value.textContent = String(widget.value ?? '');
      row.appendChild(label);
      row.appendChild(value);
      fragment.appendChild(row);
    } else if (widget.type === 'list') {
      const wrap = document.createElement('div');
      wrap.className = 'widget';
      const list = document.createElement('ul');
      (widget.items || []).forEach((item) => {
        const li = document.createElement('li');
        li.textContent = String(item);
        list.appendChild(li);
      });
      wrap.appendChild(list);
      fragment.appendChild(wrap);
    } else if (widget.type === 'progress') {
      const bar = document.createElement('div');
      bar.className = 'widget';
      const label = document.createElement('div');
      label.className = 'widget-title';
      label.textContent = widget.label || 'Progress';
      const progress = document.createElement('div');
      progress.className = 'progress-bar';
      const fill = document.createElement('div');
      fill.className = 'progress-fill';
      const pct = Math.max(0, Math.min(100, widget.percent || 0));
      fill.style.width = `${pct}%`;
      progress.appendChild(fill);
      bar.appendChild(label);
      bar.appendChild(progress);
      fragment.appendChild(bar);
    } else if (widget.type === 'text') {
      const p = document.createElement('div');
      p.className = 'widget';
      p.textContent = widget.text || '';
      fragment.appendChild(p);
    } else if (widget.type === 'code') {
      const pre = document.createElement('pre');
      pre.className = 'code-block';
      pre.textContent = widget.code || '';
      fragment.appendChild(pre);
    }
  });
  canvasRender.replaceChildren(fragment);
}

async function refreshCanvas() {
  const sessionId = sessionInput.value.trim();
  if (!sessionId) return;
  try {
    const res = await fetch(`/canvas/${encodeURIComponent(sessionId)}`, { headers: authHeaders() });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    canvasView.textContent = JSON.stringify(data.state || {}, null, 2);
    renderCanvas(data.state || {});
    setCanvasStatus('Loaded');
  } catch (err) {
    setCanvasStatus('Load failed', true);
  }
}

function setCanvasStatus(message, isError) {
  canvasStatus.textContent = message;
  canvasStatus.classList.toggle('error', Boolean(isError));
  canvasStatus.classList.toggle('success', !isError);
}

canvasSave.onclick = async () => {
  const sessionId = sessionInput.value.trim();
  if (!sessionId) return;
  let state = {};
  try {
    state = JSON.parse(canvasEditor.value || '{}');
  } catch (err) {
    setCanvasStatus('Invalid JSON', true);
    return;
  }
  await fetch(`/canvas/${encodeURIComponent(sessionId)}`, {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/json' }),
    body: JSON.stringify({ state }),
  });
  setCanvasStatus('Saved');
  refreshCanvas();
};

canvasValidate.onclick = async () => {
  const sessionId = sessionInput.value.trim();
  if (!sessionId) return;
  let state = {};
  try {
    state = JSON.parse(canvasEditor.value || '{}');
  } catch (err) {
    setCanvasStatus('Invalid JSON', true);
    return;
  }
  const res = await fetch(`/canvas/${encodeURIComponent(sessionId)}/validate`, {
    method: 'POST',
    headers: authHeaders({ 'Content-Type': 'application/json' }),
    body: JSON.stringify({ state }),
  });
  const data = await res.json();
  if (data.status === 'ok') {
    setCanvasStatus('Valid');
  } else {
    const errorCount = (data.errors || []).length;
    setCanvasStatus(`Invalid (${errorCount})`, true);
  }
};

canvasRefresh.onclick = refreshCanvas;

sendBtn.onclick = () => {
  const content = messageInput.value.trim();
  if (!content) return;
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    renderLogItem('error', 'WebSocket not connected', '', 'socket');
    return;
  }
  const payload = {
    type: 'request',
    id: `req_${Date.now()}`,
    method: 'chat.send',
    params: {
      session_id: sessionInput.value,
      channel_id: 'webchat',
      role: 'user',
      content,
      meta: { agent: 'architect', approved: true, elevated: 'full' },
    },
  };
  ws.send(JSON.stringify(payload));
  messageInput.value = '';
};

messageInput.addEventListener('keydown', (evt) => {
  if (evt.key === 'Enter') {
    evt.preventDefault();
    sendBtn.click();
  }
});

logFilter.addEventListener('change', () => {
  logFilterValue = logFilter.value;
  log.replaceChildren();
});

clearLogBtn.onclick = () => {
  log.replaceChildren();
};

sessionInput.addEventListener('change', () => {
  refreshSessions();
  refreshCanvas();
});

copyExportBtn.onclick = async () => {
  try {
    const res = await fetch('/sessions/export', { headers: authHeaders() });
    const data = await res.json();
    const payload = JSON.stringify(data, null, 2);
    await copyToClipboard(payload);
    alert('Session export copied to clipboard.');
  } catch (err) {
    alert('Failed to copy export.');
  }
};

copySessionBtn.onclick = async () => {
  const sessionId = sessionInput.value.trim();
  if (!sessionId) {
    alert('No session selected.');
    return;
  }
  try {
    const res = await fetch(`/sessions/${encodeURIComponent(sessionId)}`, { headers: authHeaders() });
    const data = await res.json();
    const payload = JSON.stringify(data, null, 2);
    await copyToClipboard(payload);
    alert('Session copied to clipboard.');
  } catch (err) {
    alert('Failed to copy session.');
  }
};

async function copyToClipboard(payload) {
  if (navigator.clipboard && navigator.clipboard.writeText) {
    await navigator.clipboard.writeText(payload);
    return;
  }
  const temp = document.createElement('textarea');
  temp.value = payload;
  document.body.appendChild(temp);
  temp.select();
  document.execCommand('copy');
  temp.remove();
}

function renderTrace() {
  const fragment = document.createDocumentFragment();
  const items = Array.from(traceState.values()).slice(-20).reverse();
  items.forEach((t) => {
    const div = document.createElement('div');
    div.className = 'item';
    div.textContent = `${t.agent_id} - ${t.status}`;
    const stamp = document.createElement('small');
    stamp.textContent = t.run_id;
    div.appendChild(stamp);
    fragment.appendChild(div);
  });
  traceEl.replaceChildren(fragment);
}

function renderSessionMeta(meta, messageCount) {
  sessionMetaEl.replaceChildren();
  const fields = [
    ['Session', meta.session_id || sessionInput.value],
    ['Channel', meta.channel_id || 'webchat'],
    ['Owner', meta.owner || 'unknown'],
    ['Scope', meta.scope || 'dm'],
    ['Messages', String(messageCount)],
    ['Updated', meta.updated_at || ''],
  ];
  const fragment = document.createDocumentFragment();
  fields.forEach(([label, value]) => {
    const row = document.createElement('div');
    row.className = 'item';
    row.textContent = `${label}: ${value}`;
    fragment.appendChild(row);
  });
  sessionMetaEl.replaceChildren(fragment);
}

function runRefreshLoop() {
  if (!hasToken() && !canUseAnonymous()) return;
  if (document.visibilityState === 'hidden') return;
  refreshSessions();
  refreshApprovals();
  refreshCanvas();
}

refreshSessionsBtn.onclick = refreshSessions;
refreshApprovalsBtn.onclick = refreshApprovals;

document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible') {
    runRefreshLoop();
  }
});

tokenInput.addEventListener('change', () => {
  connectSocket();
});

connectSocket();
setInterval(runRefreshLoop, 5000);
runRefreshLoop();

if (e2eMode) {
  window.euxisWebchat = {
    sendPayload(payload) {
      if (!ws || ws.readyState !== WebSocket.OPEN) return false;
      ws.send(JSON.stringify(payload));
      return true;
    },
    isConnected() {
      return Boolean(ws && ws.readyState === WebSocket.OPEN);
    },
  };
}
