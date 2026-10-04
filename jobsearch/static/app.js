'use strict';

async function api(path, opts) {
  const res = await fetch(path, opts);
  const data = await res.json();
  if (!res.ok) throw new Error(data.error || res.statusText);
  return data;
}
function post(path, body) {
  return api(path, {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body || {})});
}
function esc(s) { return (s || '').replace(/[&<>]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;'}[c])); }
function scoreClass(s) { return s == null ? 'score-lo' : s >= 80 ? 'score-hi' : s >= 60 ? 'score-mid' : 'score-lo'; }
function fmtDate(ts) { return ts ? new Date(ts * 1000).toLocaleDateString() : ''; }
// folder_path comes back from the DB as the real on-disk path under
// jobsearch/data/applications/ -- one place to strip that prefix down to
// what the /applications/ URL route expects, instead of the same literal
// string repeated at every call site (the kind of duplicated assumption
// that broke silently the last time this path moved).
function appDocUrl(folderPath, filename) {
  return `/applications/${encodeURIComponent(folderPath.replace('data/applications/', ''))}/${filename}`;
}

const STATUS_FLOW = ['phone_screen', 'interview', 'offer', 'rejected', 'ghosted', 'withdrawn'];
let currentPage = 1;
let currentCompany = null;      // set = flat list scoped to one company (via a card click)
let showingCompanyCards = false; // the "Group by company" toggle -- off by default, flat list is the landing view

function commonFilterParams() {
  const params = new URLSearchParams({
    eng_only: document.getElementById('f-eng').checked ? '1' : '0',
    min_score: document.getElementById('f-minscore').value || '0',
    level: document.getElementById('f-level').value,
  });
  const date = document.getElementById('f-date').value;
  if (date) params.set('date', date);
  const source = document.getElementById('f-source').value;
  if (source) params.set('source', source);
  const status = document.getElementById('f-status').value;
  if (status) params.set('status', status);
  const q = document.getElementById('f-search').value.trim();
  if (q) params.set('q', q);
  return params;
}

// ── company cards (optional, off by default) ─────────────────────────
async function loadCompanies() {
  const data = await api('/api/jobs/companies?' + commonFilterParams().toString());
  const el = document.getElementById('company-cards');
  el.innerHTML = '';
  for (const c of data.companies) {
    const card = document.createElement('div');
    card.className = 'company-card';
    const score = c.best_score == null ? '–' : Math.round(c.best_score);
    card.innerHTML = `
      <div class="cc-score ${scoreClass(c.best_score)}" style="display:inline-block;padding:2px 8px;border-radius:6px">${score}</div>
      <div class="cc-name">${esc(c.company)}</div>
      <div class="cc-meta">${c.count} matching posting${c.count === 1 ? '' : 's'}</div>
      <div class="cc-meta">${esc(c.best_title)}</div>`;
    card.onclick = () => drillIntoCompany(c.company);
    el.appendChild(card);
  }
  document.getElementById('f-count').textContent = `${data.companies.length} companies with a matching posting`;
  if (!data.companies.length) el.innerHTML = '<p class="dim">No companies match these filters.</p>';
}

function drillIntoCompany(company) {
  currentCompany = company;
  currentPage = 1;
  showingCompanyCards = false;
  document.getElementById('company-cards').hidden = true;
  document.getElementById('job-list').hidden = false;
  document.getElementById('pagination').hidden = false;
  showCompanyChip();
  loadJobs();
}

function showCompanyChip() {
  const chip = document.getElementById('company-scope-chip');
  chip.hidden = !currentCompany;
  if (currentCompany) document.getElementById('company-scope-name').textContent = currentCompany;
}

document.getElementById('btn-clear-company').onclick = () => {
  currentCompany = null;
  currentPage = 1;
  showCompanyChip();
  loadJobs();
};

document.getElementById('btn-group-by-company').onclick = () => {
  showingCompanyCards = !showingCompanyCards;
  document.getElementById('btn-group-by-company').textContent =
    showingCompanyCards ? 'Back to list' : 'Group by company';
  document.getElementById('company-cards').hidden = !showingCompanyCards;
  document.getElementById('job-list').hidden = showingCompanyCards;
  document.getElementById('pagination').hidden = showingCompanyCards;
  if (showingCompanyCards) loadCompanies(); else loadJobs();
};

// ── postings: flat list, the default view ────────────────────────────
async function loadJobs() {
  const params = commonFilterParams();
  params.set('page', currentPage);
  params.set('per_page', 10);
  if (currentCompany) params.set('company', currentCompany);

  const data = await api('/api/jobs/list?' + params.toString());
  const list = document.getElementById('job-list');
  document.getElementById('f-count').textContent =
    `${data.total_matched} matching · ${data.total_unfiltered} fetched postings total`;
  list.innerHTML = '';
  for (const j of data.jobs) {
    const row = document.createElement('div');
    row.className = 'job-row';
    // llm_score is the only score there is now -- regex only classifies
    // title/seniority/location, it doesn't rate fit. A posting with no score
    // yet just hasn't been through AI-judge.
    const isLlm = j.llm_score != null;
    const score = isLlm ? Math.round(j.llm_score) : '–';
    const tierNote = ['staff', 'senior_staff', 'manager'].includes(j.seniority_tier)
      ? `<span class="badge badge-tier">${j.seniority_tier.replace('_', ' ')}</span>` : '';
    const provenanceNote = isLlm
      ? `<span class="badge badge-llm" title="${esc(j.llm_reason || '')}">AI-judged</span>`
      : `<span class="badge badge-estimate" title="Not yet run through AI-judge">not judged</span>`;
    // Judged, but your master doc (point bank/skills/profile) changed since
    // -- this score no longer reflects your current profile. Only clears
    // when THIS posting is individually re-judged (AI-judge picks up every
    // stale posting automatically, no separate action needed).
    const staleNote = j.stale
      ? '<span class="badge badge-stale" title="Your master doc changed since this was judged -- re-run AI-judge to refresh">stale</span>' : '';
    row.innerHTML = `
      <div class="job-score ${scoreClass(j.llm_score)}" title="${isLlm ? esc(j.llm_reason || '') : 'Not yet judged'}">${score}</div>
      <div class="job-main">
        <div class="job-title">${esc(j.title)}</div>
        <div class="job-company">${esc(j.company)} · ${esc(j.source)} · ${fmtDate(j.fetched_ts)}</div>
      </div>
      <div class="job-badges">
        ${provenanceNote}
        ${staleNote}
        ${tierNote}
        ${j.applied ? '<span class="badge badge-applied">applied</span>' : ''}
        <span class="badge badge-${j.status}">${esc(j.status)}</span>
      </div>`;
    row.onclick = () => openJob(j.id);
    list.appendChild(row);
  }
  if (!data.jobs.length) list.innerHTML = '<p class="dim">No postings match these filters.</p>';

  document.getElementById('p-info').textContent = `page ${data.page} of ${data.total_pages}`;
  document.getElementById('p-prev').disabled = data.page <= 1;
  document.getElementById('p-next').disabled = data.page >= data.total_pages;
}

async function openJob(id) {
  const data = await api('/api/jobs/detail?id=' + id);
  const j = data.job, apps = data.applications;
  const panel = document.getElementById('job-detail');
  const alreadyApplied = apps.some(a => a.status !== 'generated');
  // Two shapes of application row: real generated materials (resume/cover
  // letter links + ATS estimate), or a quick "I already applied elsewhere"
  // row (empty folder_path -- no materials came out of this tool for it).
  const appsHtml = apps.length ? apps.map(a => a.folder_path ? `
    <div class="app-entry">
      <div class="app-entry-meta">
        <span class="badge badge-llm">ATS ${a.ats_estimate.score}</span>
        ${a.ats_estimate.rendered_pages ? `<span class="badge badge-new">${a.ats_estimate.rendered_pages}p</span>` : ''}
        <span class="dim">generated ${fmtDate(a.generated_ts)}</span>
        <span class="badge badge-${a.status === 'generated' ? 'new' : 'generated'}">${esc(a.status)}</span>
      </div>
      <div class="doc-links">
        <a class="doc-link" href="${appDocUrl(a.folder_path, 'resume.pdf')}">Resume <span class="ext">PDF</span></a>
        <a class="doc-link alt" href="${appDocUrl(a.folder_path, 'resume.docx')}">.docx</a>
        <a class="doc-link" href="${appDocUrl(a.folder_path, 'cover_letter.pdf')}">Cover letter <span class="ext">PDF</span></a>
        <a class="doc-link alt" href="${appDocUrl(a.folder_path, 'cover_letter.docx')}">.docx</a>
      </div>
    </div>` : `
    <div class="app-entry-applied">
      Marked applied ${fmtDate(a.applied_ts)} via ${esc(a.applied_via || '?')}${a.referral_name ? ' · referral: ' + esc(a.referral_name) : ''}
      — no materials generated through this tool — status: <b>${esc(a.status)}</b>
    </div>`).join('') : '<p class="dim" style="margin-top:8px">Not generated yet.</p>';

  panel.innerHTML = `
    <button class="close-x" onclick="closeJob()">✕</button>
    <div class="jd-title">${esc(j.title)}</div>
    <div class="jd-company">${esc(j.company)} ${j.url ? `· <a href="${esc(j.url)}" target="_blank">posting ↗</a>` : ''}</div>
    ${j.llm_score != null ? `
    <div class="score-block" style="border:1px solid var(--accent)">
      <div><div class="num">${Math.round(j.llm_score)}</div><div class="dim" style="font-size:13px">AI fit</div></div>
      <div style="flex:1">
        ${j.stale ? '<div class="badge badge-stale" style="margin-bottom:4px">stale -- your master doc changed since this was judged, re-run AI-judge to refresh</div>' : ''}
        <div style="font-size:14px">${esc(j.llm_reason || '')}</div>
      </div>
    </div>` : `
    <div class="score-block">
      <div class="dim" style="font-size:14px">Not yet AI-judged -- run "AI-judge" to get a fit score for this posting.</div>
    </div>`}
    <div class="row" style="justify-content:flex-start;gap:8px">
      <button id="jd-queue">${j.status === 'queued' ? 'Retry generating' : (j.status === 'generated' ? 'Regenerate resume + cover letter' : 'Queue for resume + cover letter')}</button>
      <button id="jd-mark-applied" ${alreadyApplied ? 'disabled' : ''} class="${alreadyApplied ? '' : 'primary'}">${alreadyApplied ? 'Applied ✓' : 'Mark as applied'}</button>
    </div>
    <div id="jd-applied-form" class="referral-form" hidden style="margin-top:10px">
      <select class="via-select">
        <option value="company_site">company site</option>
        <option value="linkedin">LinkedIn</option>
        <option value="referral">referral</option>
        <option value="recruiter">recruiter</option>
        <option value="other">other</option>
      </select>
      <input class="referral-name" type="text" placeholder="referral name (optional)">
      <input class="referral-note" type="text" placeholder="referral note (optional)">
      <button id="jd-confirm-applied" class="primary small">Confirm</button>
    </div>
    <h3 style="font-size:15px;margin-top:18px">Generated materials</h3>
    ${appsHtml}
    <h3 style="font-size:15px;margin-top:18px">Job description</h3>
    <div class="jd-body">${esc(j.description_raw)}</div>
  `;
  document.getElementById('jd-queue').onclick = async () => {
    const btn = document.getElementById('jd-queue');
    await post('/api/jobs/queue', {job_id: j.id});
    // Queuing alone used to be the whole action -- a flag with nothing
    // watching it, so nothing ever actually got generated until you asked
    // separately. Now it immediately kicks off the real draft for this job
    // (plus anything else already queued), same async batch "Generate
    // queued" uses, so there's real progress to see instead of a static
    // "queued" label with nothing happening.
    btn.disabled = true; btn.textContent = 'Starting…';
    const res = await post('/api/jobs/generate-queued', {});
    if (res.error || !res.candidates) {
      openJob(id); loadJobs();
      return;
    }
    while (true) {
      await new Promise(r => setTimeout(r, 1500));
      const st = await api('/api/jobs/generate-queued/status');
      if (!st.running) {
        flashCount(`Generated ${st.generated} of ${st.total} queued job(s).`);
        openJob(id); loadJobs();
        if (document.getElementById('view-applications').hidden === false) loadApplications();
        return;
      }
      btn.textContent = `Generating… ${st.done}/${st.total}`;
    }
  };
  if (!alreadyApplied) {
    document.getElementById('jd-mark-applied').onclick = () => {
      document.getElementById('jd-applied-form').hidden = false;
    };
    document.getElementById('jd-confirm-applied').onclick = async () => {
      await post('/api/jobs/mark-applied', {
        job_id: j.id,
        via: panel.querySelector('.via-select').value,
        referral_name: panel.querySelector('.referral-name').value.trim(),
        referral_note: panel.querySelector('.referral-note').value.trim(),
      });
      closeJob();
      loadJobs();
      if (document.getElementById('view-applications').hidden === false) loadApplications();
    };
  }
  document.getElementById('overlay').hidden = false;
  panel.hidden = false;
  document.getElementById('overlay').onclick = closeJob;
}
function closeJob() {
  document.getElementById('overlay').hidden = true;
  document.getElementById('job-detail').hidden = true;
}

// ── manual add ────────────────────────────────────────────────────────
function openManual() {
  document.getElementById('m-company').value = '';
  document.getElementById('m-url').value = '';
  document.getElementById('m-text').value = '';
  document.getElementById('m-result').innerHTML = '';
  document.getElementById('overlay').hidden = false;
  document.getElementById('manual-modal').hidden = false;
}
function closeManual() {
  document.getElementById('overlay').hidden = true;
  document.getElementById('manual-modal').hidden = true;
}
// First non-empty line is the title; the next one is the location IF it
// looks like one (short, and either "Remote"/"Hybrid"/India-ish, or a
// plain "City, Country" with no sentence punctuation) -- otherwise it's
// already prose and there's no separate location line to pull out.
function parseJobPaste(raw) {
  const lines = raw.split('\n').map(l => l.trim());
  let i = 0;
  while (i < lines.length && !lines[i]) i++;
  const title = lines[i] || '';
  let j = i + 1;
  while (j < lines.length && !lines[j]) j++;
  const candidate = lines[j] || '';
  const looksLikeLocation = candidate && candidate.length <= 60 &&
    (/remote|hybrid|on-?site|india|bangalore|bengaluru/i.test(candidate) ||
     (candidate.includes(',') && !candidate.endsWith('.')));
  return {title, location: looksLikeLocation ? candidate : ''};
}
async function submitManual() {
  const raw = document.getElementById('m-text').value.trim();
  const {title, location} = parseJobPaste(raw);
  const body = {company: document.getElementById('m-company').value.trim(), title, location,
                url: document.getElementById('m-url').value.trim(), text: raw};
  if (!body.company || !title || !raw) {
    document.getElementById('m-result').innerHTML = '<p style="color:var(--bad)">Company and the pasted posting are required.</p>';
    return;
  }
  const res = await post('/api/jobs/manual', body);
  document.getElementById('m-result').innerHTML = `
    <div class="score-block">
      <div style="flex:1">
        <div style="font-size:12px;color:var(--ok);font-weight:600">${res.is_new ? '✓ Added to your Postings list -- close this to see it (job #' + res.job_id + ')' : '✓ Already in your Postings list as job #' + res.job_id}</div>
        <div class="dim" style="font-size:12px;margin-top:2px">title: ${esc(title)}${location ? ' &middot; location: ' + esc(location) : ' &middot; no location line detected'}</div>
        <div class="dim" style="font-size:11.5px;margin-top:4px">Not scored yet -- run "AI-judge" to get a fit score for this posting.</div>
      </div>
    </div>
    <div class="row" style="justify-content:flex-start;margin-top:10px">
      <button id="m-queue" class="primary small">Queue for resume + cover letter</button>
      <button id="m-done" class="small">Close &amp; view in list</button>
    </div>`;
  document.getElementById('m-queue').onclick = async () => {
    await post('/api/jobs/queue', {job_id: res.job_id});
    const btn = document.getElementById('m-queue');
    btn.textContent = 'Queued ✓'; btn.disabled = true;
    loadJobs();
  };
  document.getElementById('m-done').onclick = () => { closeManual(); loadJobs(); };
  loadJobs();
  if (document.getElementById('view-applications').hidden === false) loadApplications();
}

// ── applications view ────────────────────────────────────────────────
async function loadApplications() {
  const data = await api('/api/applications/list');
  const list = document.getElementById('app-list');
  list.innerHTML = '';
  if (!data.applications.length) {
    list.innerHTML = '<p class="dim">No applications tracked yet.</p>';
    return;
  }
  for (const a of data.applications) {
    const div = document.createElement('div');
    div.className = 'app-row';
    const hist = (a.status_history || []).map(ev =>
      `<div>${new Date(ev.ts * 1000).toLocaleString()} — <b>${esc(ev.status)}</b> ${esc(ev.note || '')}</div>`).join('');
    // Quick-marked ("applied elsewhere, never generated through this tool")
    // rows have an empty folder_path and {} ats_estimate -- no docs to link,
    // no ATS score to show, so that whole block is skipped for those.
    div.innerHTML = `
      <div class="app-head">
        <div>
          <div class="job-title">${esc(a.title)}</div>
          <div class="job-company">${esc(a.company)}${a.folder_path ? ' · ATS ' + a.ats_estimate.score + (a.ats_estimate.rendered_pages ? ` (${a.ats_estimate.rendered_pages}p)` : '') : ' · applied directly, no materials generated here'} · status: <b>${esc(a.status)}</b></div>
        </div>
        <span class="badge badge-${a.status === 'generated' ? 'new' : 'generated'}">${esc(a.status)}</span>
      </div>
      ${a.folder_path ? `
      <div class="doc-links" style="margin-top:8px">
        <a class="doc-link" href="${appDocUrl(a.folder_path, 'resume.pdf')}">Resume <span class="ext">PDF</span></a>
        <a class="doc-link alt" href="${appDocUrl(a.folder_path, 'resume.docx')}">.docx</a>
        <a class="doc-link" href="${appDocUrl(a.folder_path, 'cover_letter.pdf')}">Cover letter <span class="ext">PDF</span></a>
        <a class="doc-link alt" href="${appDocUrl(a.folder_path, 'cover_letter.docx')}">.docx</a>
        <a class="doc-link alt" href="${appDocUrl(a.folder_path, 'job_description.md')}">Job description</a>
      </div>` : ''}
      ${a.status === 'generated' ? `
      <div class="referral-form">
        <select class="via-select">
          <option value="company_site">company site</option>
          <option value="linkedin">LinkedIn</option>
          <option value="referral">referral</option>
          <option value="recruiter">recruiter</option>
          <option value="other">other</option>
        </select>
        <input class="referral-name" type="text" placeholder="referral name (optional)">
        <input class="referral-note" type="text" placeholder="referral note (optional)">
        <button class="primary small mark-applied">Mark applied</button>
      </div>` : `
      <div class="status-track">
        ${STATUS_FLOW.map(s => `<button class="add-status" data-status="${s}">${s.replace('_',' ')}</button>`).join('')}
        <span class="dim" style="align-self:center;font-size:12px">via ${esc(a.applied_via || '?')}${a.referral_name ? ' · referral: ' + esc(a.referral_name) : ''}</span>
      </div>`}
      <div class="status-hist">${hist}</div>
    `;
    const markBtn = div.querySelector('.mark-applied');
    if (markBtn) markBtn.onclick = async () => {
      await post('/api/applications/mark-applied', {
        app_id: a.id,
        via: div.querySelector('.via-select').value,
        referral_name: div.querySelector('.referral-name').value.trim(),
        referral_note: div.querySelector('.referral-note').value.trim(),
      });
      loadApplications();
      loadJobs();
    };
    div.querySelectorAll('.add-status').forEach(btn => {
      btn.onclick = async () => {
        const note = prompt(`Note for "${btn.dataset.status}" (optional):`) || '';
        await post('/api/applications/add-status', {app_id: a.id, status: btn.dataset.status, note});
        loadApplications();
      };
    });
    list.appendChild(div);
  }
}

// ── wiring ───────────────────────────────────────────────────────────
function showView(name) {
  document.getElementById('view-postings').hidden = name !== 'postings';
  document.getElementById('view-applications').hidden = name !== 'applications';
  document.getElementById('view-profile').hidden = name !== 'profile';
  document.getElementById('tab-postings').classList.toggle('active', name === 'postings');
  document.getElementById('tab-applications').classList.toggle('active', name === 'applications');
  document.getElementById('tab-profile').classList.toggle('active', name === 'profile');
  if (name === 'applications') loadApplications();
  if (name === 'profile') {
    // reset to a clean landing every time you switch in -- upload box +
    // two buttons only, no stale proposal from a previous visit
    document.getElementById('pf-proposal-wrap').hidden = true;
    document.getElementById('pf-proposal').innerHTML = '';
    document.getElementById('pf-upload-status').textContent = '';
    document.getElementById('pf-upload').value = '';
  }
}

document.getElementById('tab-postings').onclick = () => showView('postings');
document.getElementById('tab-applications').onclick = () => showView('applications');
document.getElementById('tab-profile').onclick = () => showView('profile');
document.getElementById('btn-manual').onclick = openManual;
document.getElementById('m-cancel').onclick = closeManual;
document.getElementById('m-submit').onclick = submitManual;
function flashCount(msg) {
  const el = document.getElementById('f-count');
  const prev = el.textContent;
  el.textContent = msg;
  setTimeout(() => { if (el.textContent === msg) el.textContent = prev; }, 4000);
}
function reloadCurrentView() {
  if (showingCompanyCards) loadCompanies(); else loadJobs();
}

// Fetch only -- no LLM call here, ever. AI judging is the separate button
// below, triggered explicitly. Runs async server-side (a real fetch across
// the whole company list takes well over a minute of real HTTP calls to
// each board, too long to hold the request open for) -- start it, then
// poll /api/jobs/refetch/status the same way AI-judge polling works, so a
// reload (or just waiting) shows the real result once it lands, not a
// browser tab hung on an open connection with no feedback.
async function pollFetchStatus(btn) {
  while (true) {
    await new Promise(r => setTimeout(r, 1500));
    const st = await api('/api/jobs/refetch/status');
    if (!st.running) {
      btn.disabled = false; btn.textContent = 'Fetch new';
      flashCount(st.ok ? `${st.new_postings} new posting(s) fetched.` : 'Fetch failed -- check server logs.');
      reloadCurrentView();
      return;
    }
    btn.textContent = 'Fetching…';
  }
}
document.getElementById('btn-refetch').onclick = async () => {
  const btn = document.getElementById('btn-refetch');
  btn.disabled = true; btn.textContent = 'Fetching…';
  const res = await post('/api/jobs/refetch');
  if (res.error) { flashCount(res.error); btn.disabled = false; btn.textContent = 'Fetch new'; return; }
  pollFetchStatus(btn);
};
// The only place an AI judgment happens -- judges target-title India/
// Bangalore postings that are new, or stale (point bank changed since they
// were last judged). Can take a while for a large batch; button stays
// disabled so it's clear something's happening.
// AI-judge runs async server-side now (a real "all of them" batch is too
// many LLM calls to hold one HTTP request open for) -- start it, then poll
// /api/jobs/llm-backfill/status for real progress (each score lands in the
// db the moment that one call finishes, so "done"/"total" are real counts,
// not a guess) until the background batch reports done.
async function pollJudgeStatus(btn, defaultLabel) {
  while (true) {
    await new Promise(r => setTimeout(r, 1500));
    const st = await api('/api/jobs/llm-backfill/status');
    if (!st.running) {
      btn.disabled = false; btn.textContent = defaultLabel;
      flashCount(`AI-judged ${st.judged} of ${st.total} posting(s).`);
      reloadCurrentView();
      return;
    }
    btn.textContent = `AI-judging… ${st.done}/${st.total}`;
  }
}
document.getElementById('btn-llm-backfill').onclick = async () => {
  const btn = document.getElementById('btn-llm-backfill');
  btn.disabled = true; btn.textContent = 'Starting…';
  const res = await post('/api/jobs/llm-backfill', {});
  if (res.error) { flashCount(res.error); btn.disabled = false; btn.textContent = 'AI-judge'; return; }
  if (!res.candidates) { flashCount('Nothing new or stale to judge.'); btn.disabled = false; btn.textContent = 'AI-judge'; return; }
  pollJudgeStatus(btn, 'AI-judge');
};
// One click, not two: clearing scores alone was never useful on its own --
// the only reason to wipe them is so the next AI-judge pass re-checks
// everything, so do that pass right here instead of making it a separate
// button press. Costs a real LLM call per eligible posting -- the confirm
// text says so.
document.getElementById('btn-clear-llm').onclick = async () => {
  document.getElementById('more-actions').open = false;
  // Scoped to whatever the date filter up top is currently set to -- clearing
  // everything back to day one meant the next judge pass re-spent real LLM
  // calls on postings from months back that are probably closed by now.
  // Defaults to "fetched in the last 2 weeks" since that's the page default.
  const dateScope = document.getElementById('f-date').value;
  const scopeLabel = {'': 'ALL TIME', 'today': 'today', 'week': 'the last week', '2weeks': 'the last 2 weeks'}[dateScope] || dateScope;
  if (!confirm(`Clear AI scores and re-judge, scoped to postings fetched in ${scopeLabel}? `
    + '(change the date filter above first to widen or narrow this) Real LLM cost per posting.')) return;
  const btn = document.getElementById('btn-clear-llm');
  btn.disabled = true;
  btn.textContent = 'Clearing…';
  const cleared = await post('/api/jobs/clear-llm-scores', {date: dateScope});
  const res = await post('/api/jobs/llm-backfill', {date: dateScope});
  if (res.error) { flashCount(res.error); btn.disabled = false; btn.textContent = 'Clear & re-judge all'; return; }
  flashCount(`Cleared ${cleared.cleared}. ${res.candidates ? 'Judging ' + res.candidates + '…' : 'Nothing to judge.'}`);
  if (!res.candidates) { btn.disabled = false; btn.textContent = 'Clear & re-judge all'; reloadCurrentView(); return; }
  pollJudgeStatus(btn, 'Clear & re-judge all');
};
function resetAndLoad() {
  currentPage = 1;
  reloadCurrentView();
}
['f-search', 'f-minscore', 'f-eng', 'f-level', 'f-date', 'f-source', 'f-status'].forEach(id => {
  const el = document.getElementById(id);
  el.addEventListener('input', resetAndLoad);
  el.addEventListener('change', resetAndLoad);
});
document.getElementById('p-prev').onclick = () => { currentPage--; loadJobs(); };
document.getElementById('p-next').onclick = () => { currentPage++; loadJobs(); };

// ── profile view ─────────────────────────────────────────────────────
const FIELD_LABELS = {
  name: 'Name', title: 'Title', phone: 'Phone', email: 'Email', linkedin: 'LinkedIn',
  github: 'GitHub', education: 'Education', years_experience: 'Years experience',
  summary_current: 'Current summary',
};

async function loadProfile() {
  const [fieldsData, skillsData, pointsData] = await Promise.all([
    api('/api/profile/fields'),
    api('/api/profile/skills'),
    api('/api/profile/points?all=' + (document.getElementById('pf-show-retired').checked ? '1' : '0')),
  ]);
  renderFields(fieldsData.profile);
  renderSkills(skillsData.skills);
  renderPoints(pointsData.points);
}

function openManage() {
  document.getElementById('overlay').hidden = false;
  document.getElementById('manage-panel').hidden = false;
  loadProfile();
}
function closeManage() {
  document.getElementById('overlay').hidden = true;
  document.getElementById('manage-panel').hidden = true;
}
document.getElementById('btn-manage').onclick = openManage;

function renderFields(profile) {
  const el = document.getElementById('pf-fields');
  el.innerHTML = '';
  for (const key of Object.keys(FIELD_LABELS)) {
    const row = document.createElement('div');
    row.className = 'pf-field-row';
    const isLong = key === 'summary_current';
    row.innerHTML = `
      <label>${FIELD_LABELS[key]}</label>
      ${isLong ? `<textarea rows="3">${esc(profile[key] || '')}</textarea>`
               : `<input type="text" value="${esc(profile[key] || '')}">`}
      <button class="small">Save</button>`;
    const input = row.querySelector('input,textarea');
    row.querySelector('button').onclick = async () => {
      await post('/api/profile/fields/set', {key, value: input.value});
      flashCount(`Saved ${FIELD_LABELS[key]}.`);
    };
    el.appendChild(row);
  }
}

function renderSkills(skills) {
  const el = document.getElementById('pf-skills');
  const byCat = {};
  for (const s of skills) (byCat[s.category] ||= []).push(s.name);
  el.innerHTML = Object.keys(byCat).sort().map(cat => `
    <div class="pf-cat">
      <div class="pf-cat-name">${esc(cat)}</div>
      ${byCat[cat].sort().map(n => `<span class="pf-skill-chip">${esc(n)}</span>`).join('')}
    </div>`).join('');
}

// Keyword+Use+Result rubric, LLM rewrite-or-refuse: runs BEFORE a point-bank
// save, never invents a fact. Either proposes a rewrite (same facts, better
// shape) or says exactly what's missing so he can recall the real fact --
// shared by both the add-point modal and the Manage Entries edit flow below.
async function runRewriteCheck(text, company, role) {
  return await post('/api/profile/points/rewrite-check', {text, company, role});
}

function renderRewriteReview(panel, result, handlers) {
  panel.hidden = false;
  const body = panel.querySelector('.rewrite-body');
  const actions = panel.querySelector('.rewrite-actions');
  actions.innerHTML = '';
  const addBtn = (label, fn, primary) => {
    const btn = document.createElement('button');
    btn.className = 'small' + (primary ? ' primary' : '');
    btn.textContent = label;
    btn.onclick = fn;
    actions.appendChild(btn);
  };
  // Full response shown verbatim -- no trimming -- since the whole point of
  // asking Zai is to tell him exactly what's missing or exactly what changed.
  if (result.status === 'rewritten') {
    body.innerHTML = `<b>Proposed rewrite</b> (same facts, reshaped -- nothing added):<p class="rewrite-text">${esc(result.text)}</p>`;
    addBtn('Use this version', () => handlers.useRewritten(result.text), true);
    addBtn('Keep my original text', handlers.keepOriginal);
  } else if (result.status === 'missing_info') {
    body.innerHTML = `<b>Missing:</b> ${esc(result.explanation)}<br><b>To recall it:</b> ${esc(result.question)}`;
    addBtn('Save as written anyway', handlers.saveAnyway);
    addBtn('Let me edit it', handlers.dismiss);
  } else {
    body.innerHTML = esc(result.message || 'Check failed.');
    addBtn('Retry check', handlers.retry);
    addBtn('Save as written anyway', handlers.saveAnyway);
  }
}

function renderPoints(points) {
  const el = document.getElementById('pf-points');
  el.innerHTML = '';
  for (const p of points) {
    const row = document.createElement('div');
    row.className = 'pf-point-row' + (p.active ? '' : ' retired');
    // Keyword+Use+Result rubric, authoring-time half: visible on every real
    // point at a glance, not just the moment you happen to edit it -- so a
    // weak one doesn't sit unnoticed until it quietly gets cut from some
    // future resume. null quality = a bracket-flagged draft, not graded.
    const q = p.quality;
    const qualityNote = q && !q.ok
      ? `<div class="badge badge-stale" style="margin-top:4px">${q.weak_opener ? 'bare generic-verb opener, no brag' : 'no brag -- no number, award, or named result'}</div>`
      : '';
    row.innerHTML = `
      <div class="pf-point-head">
        <b>${esc(p.company)} / ${esc(p.role)}</b>
        <span class="meta">${esc(p.start_date)} &ndash; ${esc(p.end_date || 'present')}${p.active ? '' : ' &middot; RETIRED'}</span>
      </div>
      <textarea>${esc(p.text)}</textarea>
      ${qualityNote}
      <div class="pf-point-actions">
        <button class="small pf-save">Check &amp; save</button>
        <button class="small pf-toggle">${p.active ? 'Retire' : 'Restore'}</button>
      </div>
      <div class="pf-rewrite-panel rewrite-panel" hidden>
        <div class="rewrite-body"></div>
        <div class="rewrite-actions"></div>
      </div>`;
    const textarea = row.querySelector('textarea');
    const rewritePanel = row.querySelector('.pf-rewrite-panel');
    async function finishSavePoint(text) {
      rewritePanel.hidden = true;
      const res = await post('/api/profile/points/update', {id: p.id, text});
      flashCount(res.quality_warning
        ? `Saved -- but ${res.quality_warning.weak_opener ? 'opens on a bare generic verb with no brag' : 'has no brag (no number, award, or named result)'}.`
        : 'Point updated.');
      loadProfile();
    }
    row.querySelector('.pf-save').onclick = async () => {
      const text = textarea.value.trim();
      const result = await runRewriteCheck(text, p.company, p.role);
      renderRewriteReview(rewritePanel, result, {
        useRewritten: (newText) => { textarea.value = newText; finishSavePoint(newText); },
        keepOriginal: () => finishSavePoint(text),
        saveAnyway: () => finishSavePoint(text),
        retry: () => row.querySelector('.pf-save').click(),
        dismiss: () => { rewritePanel.hidden = true; },
      });
    };
    row.querySelector('.pf-toggle').onclick = async () => {
      await post('/api/profile/points/toggle', {id: p.id, active: !p.active});
      loadProfile();
    };
    el.appendChild(row);
  }
  if (!points.length) el.innerHTML = '<p class="dim">No points yet.</p>';
}

document.getElementById('pf-show-retired').addEventListener('change', loadProfile);

document.getElementById('pf-skill-add').onclick = async () => {
  const name = document.getElementById('pf-skill-name').value.trim();
  const cat = document.getElementById('pf-skill-cat').value.trim() || 'Other';
  if (!name) return;
  await post('/api/profile/skills/add', {name, category: cat});
  document.getElementById('pf-skill-name').value = '';
  document.getElementById('pf-skill-cat').value = '';
  loadProfile();
};

// ── add point modal ─────────────────────────────────────────────────
document.getElementById('pf-point-new').onclick = () => {
  for (const id of ['pt-company', 'pt-role', 'pt-start', 'pt-end', 'pt-text', 'pt-tags'])
    document.getElementById(id).value = '';
  document.getElementById('pt-warning').hidden = true;
  document.getElementById('pt-rewrite-panel').hidden = true;
  document.getElementById('overlay').hidden = false;
  document.getElementById('point-modal').hidden = false;
};
document.getElementById('pt-cancel').onclick = () => {
  document.getElementById('pt-warning').hidden = true;
  document.getElementById('pt-rewrite-panel').hidden = true;
  document.getElementById('overlay').hidden = true;
  document.getElementById('point-modal').hidden = true;
};
async function finishAddPoint(text) {
  document.getElementById('pt-rewrite-panel').hidden = true;
  const res = await post('/api/profile/points/add', {
    company: document.getElementById('pt-company').value.trim(),
    role: document.getElementById('pt-role').value.trim(),
    start_date: document.getElementById('pt-start').value.trim(),
    end_date: document.getElementById('pt-end').value.trim(),
    text,
    tags: document.getElementById('pt-tags').value.trim(),
  });
  loadProfile();
  const warn = document.getElementById('pt-warning');
  // Duplicate check is still independent of the rewrite-or-refuse step --
  // a bullet can be well-formed AND a near-repeat of one already on file.
  const notes = [];
  if (res.possible_duplicate_of) {
    notes.push(`looks similar to an existing point: "${res.possible_duplicate_of.slice(0, 90)}..."`);
  }
  if (res.quality_warning) {
    notes.push(res.quality_warning.weak_opener
      ? 'opens on a bare generic verb ("Implemented"/"Worked on"/...) with no brag behind it'
      : 'has no brag -- no number, award, or named result');
  }
  if (notes.length) {
    warn.hidden = false;
    warn.textContent = 'Added -- but it ' + notes.join('; and it ');
  } else {
    warn.hidden = true;
    document.getElementById('overlay').hidden = true;
    document.getElementById('point-modal').hidden = true;
  }
}
document.getElementById('pt-submit').onclick = async () => {
  const text = document.getElementById('pt-text').value.trim();
  if (!text) return;
  const company = document.getElementById('pt-company').value.trim();
  const role = document.getElementById('pt-role').value.trim();
  const panel = document.getElementById('pt-rewrite-panel');
  const result = await runRewriteCheck(text, company, role);
  renderRewriteReview(panel, result, {
    useRewritten: (newText) => { document.getElementById('pt-text').value = newText; finishAddPoint(newText); },
    keepOriginal: () => finishAddPoint(text),
    saveAnyway: () => finishAddPoint(text),
    retry: () => document.getElementById('pt-submit').click(),
    dismiss: () => { panel.hidden = true; },
  });
};

// ── upload + extract ─────────────────────────────────────────────────
function fileToBase64(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result.split(',')[1]);
    reader.onerror = reject;
    reader.readAsDataURL(file);
  });
}

async function runExtract(body, triggerBtn) {
  const status = document.getElementById('pf-upload-status');
  status.textContent = 'Extracting… (calls the LLM, can take a few seconds)';
  triggerBtn.disabled = true;
  try {
    const res = await post('/api/profile/extract', body);
    if (res.error) { status.textContent = 'Error: ' + res.error; return; }
    const dupeCount = res.proposal.points.filter(p => p.likely_duplicate).length;
    status.textContent = `Found ${res.proposal.points.length} point(s), ${res.proposal.skills.length} skill(s)`
      + (dupeCount ? ` (${dupeCount} look like duplicates already on file -- unchecked below).` : '.');
    document.getElementById('pf-proposal-wrap').hidden = false;
    renderProposal(res.proposal);
  } catch (e) {
    status.textContent = 'Error: ' + e.message;
  } finally {
    triggerBtn.disabled = false;
  }
}

document.getElementById('pf-upload-btn').onclick = async () => {
  const file = document.getElementById('pf-upload').files[0];
  const btn = document.getElementById('pf-upload-btn');
  if (!file) { document.getElementById('pf-upload-status').textContent = 'Choose a file first.'; return; }
  const content_base64 = await fileToBase64(file);
  await runExtract({filename: file.name, content_base64}, btn);
};

document.getElementById('pf-paste-toggle').onclick = () => {
  const box = document.getElementById('pf-paste-box');
  box.hidden = !box.hidden;
  document.getElementById('pf-paste-toggle').textContent = box.hidden ? 'or paste text instead →' : '← or upload a file instead';
};
document.getElementById('pf-paste-btn').onclick = async () => {
  const text = document.getElementById('pf-paste-text').value.trim();
  const btn = document.getElementById('pf-paste-btn');
  if (!text) { document.getElementById('pf-upload-status').textContent = 'Paste some text first.'; return; }
  await runExtract({text}, btn);
};

function renderProposal(proposal) {
  const el = document.getElementById('pf-proposal');
  el.innerHTML = `
    <h3 style="font-size:13px;margin-top:16px">Proposed points (uncheck to skip)</h3>
    <div id="pf-proposal-points"></div>
    <h3 style="font-size:13px;margin-top:16px">Proposed skills (uncheck to skip)</h3>
    <div id="pf-proposal-skills"></div>
    <button id="pf-commit" class="primary" style="margin-top:12px">Add to master doc</button>
  `;
  const pointsEl = document.getElementById('pf-proposal-points');
  proposal.points.forEach((p, i) => {
    const row = document.createElement('div');
    row.className = 'pf-proposal-row';
    // likely_duplicate comes from the server (difflib against what's
    // already in the point bank) -- default the checkbox OFF for those so
    // committing a near-duplicate takes a deliberate re-check, not a miss.
    const dupeBadge = p.likely_duplicate
      ? `<span class="dupe-badge" title="Close match: ${esc(p.duplicate_of_text || '')}">possible duplicate</span><br>` : '';
    row.innerHTML = `
      <input type="checkbox" ${p.likely_duplicate ? '' : 'checked'} data-idx="${i}" class="prop-point-check">
      <div style="flex:1">
        ${dupeBadge}
        <div class="dim" style="font-size:11.5px">${esc(p.company)} / ${esc(p.role)} &middot; ${esc(p.start_date || '?')}&ndash;${esc(p.end_date || 'present')}</div>
        <textarea data-idx="${i}" class="prop-point-text">${esc(p.text)}</textarea>
      </div>`;
    pointsEl.appendChild(row);
  });
  const skillsEl = document.getElementById('pf-proposal-skills');
  proposal.skills.forEach((s, i) => {
    const row = document.createElement('label');
    row.style.cssText = 'display:inline-flex;align-items:center;gap:5px;margin:2px 10px 2px 0;font-size:12.5px';
    row.innerHTML = `<input type="checkbox" checked data-idx="${i}" class="prop-skill-check"> ${esc(s.name)} <span class="dim">(${esc(s.category)})</span>`;
    skillsEl.appendChild(row);
  });

  document.getElementById('pf-commit').onclick = async () => {
    const points = [];
    document.querySelectorAll('.prop-point-check').forEach(cb => {
      if (cb.checked) {
        const i = cb.dataset.idx;
        const text = document.querySelector(`.prop-point-text[data-idx="${i}"]`).value;
        points.push({...proposal.points[i], text});
      }
    });
    const skills = [];
    document.querySelectorAll('.prop-skill-check').forEach(cb => {
      if (cb.checked) skills.push(proposal.skills[cb.dataset.idx]);
    });
    const res = await post('/api/profile/extract/commit', {points, skills});
    document.getElementById('pf-upload-status').textContent =
      `Added ${res.added_points} point(s), ${res.added_skills} skill(s) to the master doc`
      + (res.skipped_dupes ? ` (${res.skipped_dupes} skipped as duplicates).` : '.');
    document.getElementById('pf-proposal-wrap').hidden = true;
    document.getElementById('pf-proposal').innerHTML = '';
    document.getElementById('pf-upload').value = '';
  };
}

// ── master document ─────────────────────────────────────────────────
// A read-only, single-page view of literally everything on file -- the
// point bank scattered across many small edit rows is good for editing,
// bad for "did I forget something" or "what was that number again."
// Groups every point (including retired and bracket-flagged ones, clearly
// marked) by company/role, plus the full skill list and profile fields.
async function openMasterDoc() {
  const [fieldsData, skillsData, pointsData] = await Promise.all([
    api('/api/profile/fields'),
    api('/api/profile/skills'),
    api('/api/profile/points?all=1'),
  ]);
  const profile = fieldsData.profile;
  const points = pointsData.points;

  // Master doc shows what's actually current -- a retired point is gone
  // from here the moment it's retired, not crossed out and left visible.
  // The point bank's own "show retired" toggle is the place to still see
  // retired entries when you need to.
  const activePoints = points.filter(p => p.active);

  const roles = [];
  const seen = new Map();
  for (const p of activePoints) {
    const key = p.company + '|' + p.role;
    if (!seen.has(key)) {
      seen.set(key, {company: p.company, role: p.role, start: p.start_date, end: p.end_date, points: []});
      roles.push(seen.get(key));
    }
    seen.get(key).points.push(p);
  }

  const flaggedCount = activePoints.filter(p => p.text.trim().startsWith('[')).length;
  const retiredCount = points.filter(p => !p.active).length;

  const byCat = {};
  for (const s of skillsData.skills) (byCat[s.category] ||= []).push(s.name);

  const rolesHtml = roles.map(r => `
    <div class="md-role">
      <div class="md-role-head"><span>${esc(r.role)}</span><span class="dim">${esc(r.start)} &ndash; ${esc(r.end || 'present')}</span></div>
      <div class="md-role-company">${esc(r.company)}</div>
      ${r.points.map(p => {
        const flagged = p.text.trim().startsWith('[');
        const note = flagged ? '<span class="md-flag-note">NEEDS YOUR REVIEW</span>' : '';
        return `<div class="md-bullet ${flagged ? 'flagged' : ''}">${esc(p.text)}${note}</div>`;
      }).join('')}
    </div>`).join('');

  const skillsHtml = Object.keys(byCat).sort().map(cat =>
    `<div class="md-skills-cat"><b>${esc(cat)}:</b> ${byCat[cat].sort().join(', ')}</div>`).join('');

  document.getElementById('masterdoc-body').innerHTML = `
    <div class="md-header">
      <h1>${esc(profile.name || '')}</h1>
      <div class="contact">${esc(profile.title || '')} &middot; ${esc(profile.phone || '')} &middot; ${esc(profile.email || '')}
        &middot; ${esc(profile.linkedin || '')} &middot; ${esc(profile.github || '')}</div>
    </div>
    <div class="md-summary">${esc(profile.summary_current || '')}</div>
    <p class="dim" style="font-size:12.5px">
      ${activePoints.length} point(s) shown &middot;
      ${retiredCount ? `${retiredCount} retired (hidden here -- see point bank) &middot; ` : ''}
      ${flaggedCount ? `<b style="color:var(--warn)">${flaggedCount} flagged, needs your review</b>` : 'nothing flagged'}
    </p>
    <h3 style="font-size:14px;margin-top:20px">EXPERIENCE</h3>
    ${rolesHtml}
    <h3 style="font-size:14px;margin-top:20px">SKILLS</h3>
    ${skillsHtml}
    <h3 style="font-size:14px;margin-top:20px">EDUCATION</h3>
    <div>${esc(profile.education || '')}</div>
  `;
  document.getElementById('overlay').hidden = false;
  document.getElementById('masterdoc-panel').hidden = false;
}
function closeMasterDoc() {
  document.getElementById('overlay').hidden = true;
  document.getElementById('masterdoc-panel').hidden = true;
}
document.getElementById('btn-masterdoc').onclick = openMasterDoc;

// The "More" menu is a native <details>, which only closes on its own
// summary click -- clicking anywhere else on the page left it hanging open.
document.addEventListener('click', (e) => {
  const more = document.getElementById('more-actions');
  if (more.open && !more.contains(e.target)) more.open = false;
});

loadJobs();
