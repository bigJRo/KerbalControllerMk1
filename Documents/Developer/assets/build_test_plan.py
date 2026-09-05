#!/usr/bin/env python3
"""Build the interactive checklist page from Display_Test_Plan.md.

    python3 Documents/Developer/assets/build_test_plan.py

Reads ../Display_Test_Plan.md, writes ../Display_Test_Plan.html next to it: a
single self-contained page where every test point carries a PASS / FAIL / N/A
control and a note. Results persist in the browser (localStorage) and, when the
page is published as a claude.ai artifact with the `db` capability, in that
artifact's shared store so a run survives across devices.

`--fragment PATH` also writes the page without the <!doctype>/<html>/<head>/<body>
skeleton, which is the form the Artifact tool expects.

The markdown is the source of truth: edit it, re-run this script, commit both.
"""
import html
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MD = HERE.parent / "Display_Test_Plan.md"
OUT = HERE.parent / "Display_Test_Plan.html"

# ── inline markdown ──────────────────────────────────────────────────────────

def inline(s: str) -> str:
    s = html.escape(s, quote=False)
    s = s.replace("\\|", "|")
    s = re.sub(r"`([^`]+)`", r"<code>\1</code>", s)
    s = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", s)
    s = re.sub(r"(?<![\w*])\*([^*]+)\*(?![\w*])", r"<em>\1</em>", s)
    s = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', s)
    return s


def split_row(line: str):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [c.strip() for c in re.split(r"(?<!\\)\|", line)]

# ── block parse ──────────────────────────────────────────────────────────────

def parse(md: str):
    """Return a list of top-level sections, each with a title and blocks."""
    lines = md.splitlines()
    sections = []
    cur = None
    i = 0

    def new_section(level, title):
        nonlocal cur
        sec = {"level": level, "title": title, "blocks": []}
        sections.append(sec)
        cur = sec

    while i < len(lines):
        ln = lines[i]
        if ln.startswith("# "):
            i += 1
            continue
        m = re.match(r"^(##+)\s+(.*)$", ln)
        if m:
            new_section(len(m.group(1)), m.group(2).strip())
            i += 1
            continue
        if ln.strip() == "---" or ln.strip() == "":
            i += 1
            continue
        if cur is None:
            new_section(2, "Front matter")
        if ln.lstrip().startswith("|"):
            rows = []
            while i < len(lines) and lines[i].lstrip().startswith("|"):
                rows.append(split_row(lines[i]))
                i += 1
            header = rows[0]
            body = [r for r in rows[2:]]  # skip separator
            cur["blocks"].append({"t": "table", "header": header, "rows": body})
            continue
        if re.match(r"^\s*[-*]\s+", ln):
            items = []
            while i < len(lines) and re.match(r"^\s*[-*]\s+", lines[i]):
                items.append(re.sub(r"^\s*[-*]\s+", "", lines[i]))
                i += 1
            cur["blocks"].append({"t": "ul", "items": items})
            continue
        if re.match(r"^\s*\d+\.\s+", ln):
            items = []
            while i < len(lines) and re.match(r"^\s*\d+\.\s+", lines[i]):
                items.append(re.sub(r"^\s*\d+\.\s+", "", lines[i]))
                i += 1
            cur["blocks"].append({"t": "ol", "items": items})
            continue
        if ln.startswith("> "):
            cur["blocks"].append({"t": "p", "text": ln[2:]})
            i += 1
            continue
        # paragraph
        para = [ln]
        i += 1
        while i < len(lines) and lines[i].strip() and not lines[i].lstrip().startswith(("|", "#", "-", "*", ">")) and not re.match(r"^\s*\d+\.\s+", lines[i]):
            para.append(lines[i])
            i += 1
        cur["blocks"].append({"t": "p", "text": " ".join(para)})
    return sections

# ── model ────────────────────────────────────────────────────────────────────

SESSION_RE = re.compile(r"^(\d+)\.\s+Session\s+([A-Z/&]+)\s+[—-]\s+(.*)$")
SUB_RE = re.compile(r"^(\d+\.\d+)\s+(.*)$")


def slug(s: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")


def build_model(sections):
    """Group sections into nav groups; extract test points."""
    groups = []   # {id, key, title, blocks:[...], subs:[...]}
    points = []   # {id, group, action, expected, source}
    front = []    # blocks before the first Session
    g = None
    for sec in sections:
        title = sec["title"]
        m = SESSION_RE.match(title)
        if sec["level"] == 2 and m:
            g = {"id": slug(m.group(2)), "key": m.group(2), "num": int(m.group(1)),
                 "title": m.group(3), "blocks": [], "subs": []}
            groups.append(g)
            target = g["blocks"]
        elif sec["level"] == 2:
            # non-session top-level section (front matter, appendices)
            g = {"id": slug(title), "key": None, "num": None, "title": re.sub(r"^\d+\.\s+", "", title),
                 "blocks": [], "subs": [], "plain": True}
            groups.append(g)
            target = g["blocks"]
        elif sec["level"] == 3 and g is not None:
            sm = SUB_RE.match(title)
            sub = {"id": slug(title), "title": sm.group(2) if sm else title, "blocks": []}
            g["subs"].append(sub)
            target = sub["blocks"]
        else:
            continue
        for b in sec["blocks"]:
            if b["t"] == "table" and b["header"] and b["header"][0] == "ID" and len(b["header"]) == 4 and b["header"][2] == "Expected":
                ids = []
                for r in b["rows"]:
                    if len(r) < 4:
                        continue
                    pid = r[0].strip("` ")
                    if not re.match(r"^[A-Z]+-\d+$", pid):
                        continue
                    pt = {"id": pid, "group": g["id"], "action": inline(r[1]), "expected": inline(r[2]), "source": inline(r[3])}
                    points.append(pt)
                    ids.append(pid)
                target.append({"t": "points", "ids": ids, "header": b["header"]})
            else:
                target.append(b)
    return groups, points

# ── render ───────────────────────────────────────────────────────────────────

def render_block(b):
    if b["t"] == "p":
        return f"<p>{inline(b['text'])}</p>"
    if b["t"] == "ul":
        return "<ul>" + "".join(f"<li>{inline(x)}</li>" for x in b["items"]) + "</ul>"
    if b["t"] == "ol":
        return "<ol>" + "".join(f"<li>{inline(x)}</li>" for x in b["items"]) + "</ol>"
    if b["t"] == "table":
        head = "".join(f"<th>{inline(h)}</th>" for h in b["header"])
        rows = "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>" for r in b["rows"] if any(r))
        return f'<div class="tablewrap"><table><thead><tr>{head}</tr></thead><tbody>{rows}</tbody></table></div>'
    if b["t"] == "points":
        cols = b["header"]
        head = (f'<div class="tp tp-head" role="row"><div>{inline(cols[0])}</div><div>{inline(cols[1])}</div>'
                f'<div>{inline(cols[2])}</div><div>{inline(cols[3])}</div><div>Result</div></div>')
        body = "".join(f'<div class="tp-slot" data-id="{pid}"></div>' for pid in b["ids"])
        return f'<div class="tplist" role="table">{head}{body}</div>'
    return ""


def render_group(g):
    out = []
    plain = g.get("plain", False)
    key = g["key"]
    head_num = f'<span class="gnum">{g["num"]:02d}</span>' if g["num"] else ""
    key_html = f'<span class="gkey">{html.escape(key)}</span>' if key else ""
    prog = "" if plain else f'<div class="gprog" data-group="{g["id"]}"><span class="gbar"><i></i></span><span class="gcount"></span></div>'
    out.append(f'<section class="group{" plain" if plain else ""}" id="{g["id"]}" data-group="{g["id"]}">')
    out.append(f'<header class="ghead"><h2>{head_num}{key_html}<span class="gtitle">{html.escape(g["title"])}</span></h2>{prog}</header>')
    for b in g["blocks"]:
        out.append(render_block(b))
    for s in g["subs"]:
        out.append(f'<h3 id="{s["id"]}">{html.escape(s["title"])}</h3>')
        for b in s["blocks"]:
            out.append(render_block(b))
    out.append("</section>")
    return "\n".join(out)


def render_nav(groups):
    items = []
    for g in groups:
        if g["id"] == "front-matter":
            continue
        if g.get("plain"):
            items.append(f'<a class="nav-item plain" href="#{g["id"]}"><span class="nav-title">{html.escape(g["title"])}</span></a>')
        else:
            short = re.sub(r"\s*\([^)]*\)", "", g["title"])
            items.append(f'<a class="nav-item" href="#{g["id"]}" data-group="{g["id"]}"><span class="nav-key">{html.escape(g["key"])}</span>'
                         f'<span class="nav-title">{html.escape(short)}</span><span class="nav-count"></span></a>')
    return "\n".join(items)


CSS = r"""
<style>
:root{
  --ground:#F4F6F5; --panel:#FFFFFF; --panel-2:#EAEEEC; --line:#CFD6D3; --line-2:#B7C0BC;
  --ink:#141B1E; --ink-2:#4A575D; --ink-3:#7B878C;
  --accent:#0E8FB3; --accent-ink:#FFFFFF; --accent-soft:#D7EEF5;
  --pass:#2E8B45; --pass-soft:#DDF1E2; --fail:#C8321E; --fail-soft:#F8DEDA; --na:#7B878C; --na-soft:#E6EAE8;
  --code-bg:#E8ECEA; --shadow:0 1px 2px rgba(20,27,30,.08);
  --font-display:"Barlow Condensed","Arial Narrow",Arial,sans-serif;
  --font-body:"IBM Plex Sans","Segoe UI",Roboto,Helvetica,Arial,sans-serif;
  --font-mono:"IBM Plex Mono",ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
}
@media (prefers-color-scheme: dark){
  :root:not([data-theme="light"]){
    --ground:#0F1416; --panel:#161C1F; --panel-2:#1E2629; --line:#2A3336; --line-2:#3A4549;
    --ink:#E6ECEA; --ink-2:#A9B5B2; --ink-3:#77837F;
    --accent:#3FB8DB; --accent-ink:#0B1316; --accent-soft:#12333D;
    --pass:#4CC466; --pass-soft:#153420; --fail:#F0644E; --fail-soft:#3F1A14; --na:#8B9793; --na-soft:#252D2F;
    --code-bg:#1F282B; --shadow:none;
  }
}
:root[data-theme="dark"]{
  --ground:#0F1416; --panel:#161C1F; --panel-2:#1E2629; --line:#2A3336; --line-2:#3A4549;
  --ink:#E6ECEA; --ink-2:#A9B5B2; --ink-3:#77837F;
  --accent:#3FB8DB; --accent-ink:#0B1316; --accent-soft:#12333D;
  --pass:#4CC466; --pass-soft:#153420; --fail:#F0644E; --fail-soft:#3F1A14; --na:#8B9793; --na-soft:#252D2F;
  --code-bg:#1F282B; --shadow:none;
}
*{box-sizing:border-box}
html{scroll-padding-top:84px}
body{margin:0;background:var(--ground);color:var(--ink);font-family:var(--font-body);font-size:14px;line-height:1.45;-webkit-font-smoothing:antialiased}
a{color:var(--accent)}
code{font-family:var(--font-mono);font-size:.92em;background:var(--code-bg);padding:.05em .35em;border-radius:3px}
h1,h2,h3{font-family:var(--font-display);text-wrap:balance;margin:0}
strong{font-weight:600}
table{font:inherit;border-collapse:collapse;width:100%}
.tablewrap{overflow-x:auto;margin:10px 0 18px;border:1px solid var(--line);border-radius:6px;background:var(--panel)}
th,td{text-align:left;vertical-align:top;padding:8px 10px;border-bottom:1px solid var(--line)}
th{font-family:var(--font-display);font-size:14px;letter-spacing:.06em;text-transform:uppercase;color:var(--ink-2);background:var(--panel-2)}
tr:last-child td{border-bottom:0}
p{max-width:72ch;margin:8px 0}
ul,ol{max-width:72ch;margin:8px 0;padding-left:22px}
li{margin:4px 0}

/* top bar */
.topbar{position:sticky;top:0;z-index:20;background:var(--panel);border-bottom:1px solid var(--line);box-shadow:var(--shadow)}
.topbar-in{display:flex;align-items:center;gap:18px;padding:10px 22px;flex-wrap:wrap}
.brand{display:flex;flex-direction:column;line-height:1.05}
.brand h1{font-size:24px;font-weight:600;letter-spacing:.02em}
.brand small{font-family:var(--font-mono);font-size:11px;color:var(--ink-3);letter-spacing:.04em;margin-top:2px}
.progress{flex:1;min-width:220px;display:flex;flex-direction:column;gap:4px}
.pbar{height:8px;border-radius:4px;background:var(--panel-2);overflow:hidden;display:flex}
.pbar i{display:block;height:100%}
.pbar .p{background:var(--pass)} .pbar .f{background:var(--fail)} .pbar .n{background:var(--na)}
.pstats{display:flex;gap:14px;font-family:var(--font-mono);font-size:11.5px;color:var(--ink-2);font-variant-numeric:tabular-nums}
.pstats b{color:var(--ink);font-weight:600}
.pstats .dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:5px;vertical-align:middle}
.tools{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.seg{display:inline-flex;border:1px solid var(--line-2);border-radius:6px;overflow:hidden}
.seg button{appearance:none;border:0;background:transparent;color:var(--ink-2);font:inherit;font-size:12.5px;padding:5px 10px;cursor:pointer}
.seg button+button{border-left:1px solid var(--line-2)}
.seg button[aria-pressed="true"]{background:var(--accent);color:var(--accent-ink)}
.btn{appearance:none;border:1px solid var(--line-2);background:var(--panel);color:var(--ink);font:inherit;font-size:12.5px;padding:5px 11px;border-radius:6px;cursor:pointer}
.btn:hover{border-color:var(--accent)}
.btn.danger:hover{border-color:var(--fail);color:var(--fail)}
.sync{font-family:var(--font-mono);font-size:11px;color:var(--ink-3);display:inline-flex;align-items:center;gap:6px;white-space:nowrap}
.sync .dot{width:8px;height:8px;border-radius:50%;background:var(--na)}
.sync.ok .dot{background:var(--pass)} .sync.busy .dot{background:var(--accent)} .sync.err .dot{background:var(--fail)}
button:focus-visible,input:focus-visible,textarea:focus-visible,a:focus-visible{outline:2px solid var(--accent);outline-offset:2px}

/* layout */
.wrap{display:grid;grid-template-columns:250px minmax(0,1fr);gap:0;max-width:1480px;margin:0 auto}
.nav{position:sticky;top:64px;align-self:start;max-height:calc(100vh - 64px);overflow-y:auto;padding:16px 10px 30px 18px;border-right:1px solid var(--line)}
.nav-label{font-family:var(--font-display);font-size:12px;letter-spacing:.14em;text-transform:uppercase;color:var(--ink-3);padding:6px 8px}
.nav-item{display:grid;grid-template-columns:52px 1fr auto;align-items:baseline;gap:6px;padding:6px 8px;border-radius:5px;text-decoration:none;color:var(--ink)}
.nav-item.plain{grid-template-columns:1fr;color:var(--ink-2)}
.nav-item:hover{background:var(--panel-2)}
.nav-item.active{background:var(--accent-soft)}
.nav-key{font-family:var(--font-mono);font-size:11.5px;color:var(--accent);font-weight:600}
.nav-title{font-size:13px}
.nav-count{font-family:var(--font-mono);font-size:11px;color:var(--ink-3);font-variant-numeric:tabular-nums}
.nav-item.done .nav-count{color:var(--pass)}
.nav-item.hasfail .nav-count{color:var(--fail)}
main{padding:18px 26px 80px;min-width:0}

/* groups */
.group{margin:0 0 34px;scroll-margin-top:84px}
.ghead{display:flex;align-items:flex-end;justify-content:space-between;gap:16px;border-bottom:2px solid var(--ink);padding-bottom:6px;margin:8px 0 12px;flex-wrap:wrap}
.ghead h2{font-size:30px;font-weight:600;display:flex;align-items:baseline;gap:10px;letter-spacing:.01em}
.gnum{font-family:var(--font-mono);font-size:14px;color:var(--ink-3);font-weight:400}
.gkey{font-family:var(--font-mono);font-size:15px;color:var(--accent);font-weight:600;letter-spacing:.04em}
.group.plain .ghead{border-bottom-color:var(--line-2)}
.group.plain h2{font-size:24px;color:var(--ink-2)}
h3{font-size:20px;font-weight:600;margin:20px 0 8px;color:var(--ink-2)}
.gprog{display:flex;align-items:center;gap:10px;font-family:var(--font-mono);font-size:11.5px;color:var(--ink-2);font-variant-numeric:tabular-nums}
.gbar{width:140px;height:6px;border-radius:3px;background:var(--panel-2);overflow:hidden;display:block}
.gbar i{display:block;height:100%;width:0;background:var(--pass)}

/* test points */
.tplist{border:1px solid var(--line);border-radius:6px;background:var(--panel);margin:8px 0 18px;overflow:hidden}
.tp{display:grid;grid-template-columns:92px minmax(160px,1.1fr) minmax(200px,1.7fr) 150px 190px;gap:0 14px;padding:9px 12px;border-top:1px solid var(--line);align-items:start}
.tp-head{border-top:0;background:var(--panel-2);font-family:var(--font-display);font-size:13px;letter-spacing:.08em;text-transform:uppercase;color:var(--ink-2);padding:7px 12px}
.tp .id{font-family:var(--font-mono);font-size:12px;font-weight:600;color:var(--ink);padding-top:3px;display:flex;align-items:center;gap:7px}
.tp .id .mark{width:14px;height:14px;border-radius:50%;border:1.5px solid var(--line-2);flex:none;display:inline-grid;place-items:center;font-size:10px;line-height:1}
.tp .src{font-family:var(--font-mono);font-size:11px;color:var(--ink-3);padding-top:4px;word-break:break-word}
.tp .act,.tp .exp{font-size:13.5px}
.tp .exp{color:var(--ink-2)}
.tp .exp strong{color:var(--ink)}
.tp .res{display:flex;flex-direction:column;gap:6px;align-items:flex-start}
.tri{display:inline-flex;border:1px solid var(--line-2);border-radius:6px;overflow:hidden}
.tri button{appearance:none;border:0;background:transparent;color:var(--ink-2);font-family:var(--font-display);font-size:14px;letter-spacing:.06em;padding:4px 0;width:50px;cursor:pointer}
.tri button+button{border-left:1px solid var(--line-2)}
.tri button:hover{background:var(--panel-2)}
.tri button[aria-pressed="true"].p{background:var(--pass);color:#fff}
.tri button[aria-pressed="true"].f{background:var(--fail);color:#fff}
.tri button[aria-pressed="true"].n{background:var(--na);color:#fff}
.notebtn{appearance:none;border:0;background:none;color:var(--accent);font:inherit;font-size:11.5px;padding:0;cursor:pointer;text-decoration:underline dotted}
.note{width:100%;min-height:34px;resize:vertical;font:inherit;font-size:12.5px;padding:5px 7px;border:1px solid var(--line-2);border-radius:5px;background:var(--panel);color:var(--ink)}
.tp[data-s="P"]{background:color-mix(in srgb,var(--pass-soft) 45%,transparent)}
.tp[data-s="P"] .act,.tp[data-s="P"] .exp{color:var(--ink-3)}
.tp[data-s="P"] .id .mark{background:var(--pass);border-color:var(--pass);color:#fff}
.tp[data-s="F"]{background:color-mix(in srgb,var(--fail-soft) 55%,transparent)}
.tp[data-s="F"] .id .mark{background:var(--fail);border-color:var(--fail);color:#fff}
.tp[data-s="NA"] .act,.tp[data-s="NA"] .exp{color:var(--ink-3)}
.tp[data-s="NA"] .id .mark{background:var(--na);border-color:var(--na);color:#fff}
.tp.hidden{display:none}
.tplist.allhidden{display:none}

/* export */
.export{border:1px solid var(--line);border-radius:6px;background:var(--panel);padding:14px 16px;margin:8px 0 18px;max-width:72ch}
.export textarea{width:100%;min-height:180px;font-family:var(--font-mono);font-size:12px;border:1px solid var(--line-2);border-radius:5px;background:var(--panel-2);color:var(--ink);padding:8px;margin-top:10px}
.export .row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}

@media (max-width:1100px){
  .tp{grid-template-columns:84px minmax(0,1fr) 170px;grid-template-areas:"id act res" "id exp res" "id src res"}
  .tp .id{grid-area:id}.tp .act{grid-area:act}.tp .exp{grid-area:exp}.tp .src{grid-area:src}.tp .res{grid-area:res}
  .tp-head div:nth-child(3),.tp-head div:nth-child(4){display:none}
  .tp-head{grid-template-areas:"id act res"}
}
@media (max-width:820px){
  .wrap{grid-template-columns:1fr}
  .nav{position:static;max-height:none;border-right:0;border-bottom:1px solid var(--line);display:flex;flex-wrap:wrap;gap:4px;padding:10px 12px}
  .nav-label{width:100%}
  .nav-item{grid-template-columns:auto auto;padding:4px 8px;border:1px solid var(--line);border-radius:14px}
  .nav-item .nav-title{display:none}
  .nav-item.plain{grid-template-columns:1fr}.nav-item.plain .nav-title{display:inline}
  main{padding:14px 12px 80px}
  .tp{grid-template-columns:1fr;grid-template-areas:"id" "act" "exp" "src" "res";gap:4px}
  .tp-head{display:none}
  .ghead h2{font-size:24px}
}
@media (prefers-reduced-motion:no-preference){
  .gbar i,.pbar i{transition:width .25s ease}
}
</style>
"""

JS = r"""
<script>
(function(){
  const POINTS = window.__POINTS__;
  const GROUPS = window.__GROUPS__;
  const STORE_KEY = 'kcmk1-display-test-plan-v1';
  const byId = Object.fromEntries(POINTS.map(p => [p.id, p]));
  let state = { points: {}, updatedAt: null };
  let filter = 'all';
  let db = null, doc = null, writeTimer = null, applyingRemote = false;

  // ── persistence: local first, artifact db when the viewer grants it ──
  function loadLocal(){ try { const raw = localStorage.getItem(STORE_KEY); if (raw) { const s = JSON.parse(raw); if (s && s.points) state = s; } } catch(e){} }
  function saveLocal(){ try { localStorage.setItem(STORE_KEY, JSON.stringify(state)); } catch(e){} }
  function setSync(cls, text){ const el = document.getElementById('sync'); el.className = 'sync ' + cls; el.querySelector('span').textContent = text; }

  function persist(){
    state.updatedAt = new Date().toISOString();
    saveLocal();
    if (doc) {
      setSync('busy', 'Saving…');
      clearTimeout(writeTimer);
      writeTimer = setTimeout(() => {
        doc.set(JSON.parse(JSON.stringify(state))).then(() => setSync('ok', 'Saved to this artifact'))
          .catch(e => setSync('err', 'Save failed (' + (e && e.code || 'error') + '); kept in this browser'));
      }, 500);
    } else {
      setSync('ok', 'Saved in this browser');
    }
  }

  async function connectDb(){
    if (!(window.claude && typeof window.claude.use === 'function')) { setSync('ok', 'Saved in this browser'); return; }
    try { db = await window.claude.use('db'); } catch(e) { db = null; }
    if (!db) { setSync('ok', 'Saved in this browser'); return; }
    doc = db.doc('runs/current');
    let first = true;
    doc.onSnapshot(snap => {
      if (snap.exists) {
        const remote = snap.data();
        if (remote && remote.points) {
          const localNewer = first && state.updatedAt && remote.updatedAt && state.updatedAt > remote.updatedAt && Object.keys(state.points).length;
          if (!localNewer) {
            applyingRemote = true;
            state = JSON.parse(JSON.stringify(remote));
            saveLocal();
            renderAll();
            applyingRemote = false;
          } else {
            persist();
          }
        }
        setSync('ok', snap.metadata.hasPendingWrites ? 'Saving…' : 'Synced with this artifact');
      } else if (first && Object.keys(state.points).length) {
        persist();
      } else {
        setSync('ok', 'Synced with this artifact');
      }
      first = false;
    }, err => setSync('err', 'Live sync lost; saving in this browser'));
  }

  // ── rendering ──
  function get(id){ return state.points[id] || {}; }
  function set(id, patch){
    const cur = Object.assign({}, get(id), patch);
    if (!cur.s && !cur.note) delete state.points[id]; else state.points[id] = cur;
    persist();
  }

  function rowEl(p){
    const r = get(p.id);
    const el = document.createElement('div');
    el.className = 'tp';
    el.setAttribute('role', 'row');
    el.dataset.id = p.id;
    if (r.s) el.dataset.s = r.s;
    const mark = r.s === 'P' ? '✓' : r.s === 'F' ? '✕' : r.s === 'NA' ? '–' : '';
    el.innerHTML =
      '<div class="id"><span class="mark" aria-hidden="true">' + mark + '</span>' + p.id + '</div>' +
      '<div class="act">' + p.action + '</div>' +
      '<div class="exp">' + p.expected + '</div>' +
      '<div class="src">' + p.source + '</div>' +
      '<div class="res">' +
        '<div class="tri" role="group" aria-label="Result for ' + p.id + '">' +
          '<button class="p" aria-pressed="' + (r.s==='P') + '" data-v="P" title="Pass">PASS</button>' +
          '<button class="f" aria-pressed="' + (r.s==='F') + '" data-v="F" title="Fail">FAIL</button>' +
          '<button class="n" aria-pressed="' + (r.s==='NA') + '" data-v="NA" title="Not applicable">N/A</button>' +
        '</div>' +
        (r.s === 'F' || r.note ? '<textarea class="note" placeholder="What happened? (kept with the run log)" aria-label="Note for ' + p.id + '">' + escapeHtml(r.note || '') + '</textarea>'
                                : '<button class="notebtn" type="button">+ note</button>') +
      '</div>';
    el.querySelectorAll('.tri button').forEach(b => b.addEventListener('click', () => {
      const v = b.dataset.v;
      set(p.id, { s: get(p.id).s === v ? '' : v });
      renderRow(p.id);
      renderCounts();
    }));
    const nb = el.querySelector('.notebtn');
    if (nb) nb.addEventListener('click', () => { set(p.id, { note: get(p.id).note || '' }); renderRow(p.id, true); });
    const ta = el.querySelector('.note');
    if (ta) ta.addEventListener('input', () => { set(p.id, { note: ta.value }); });
    applyFilter(el);
    return el;
  }

  function renderRow(id, focusNote){
    const p = byId[id];
    const old = document.querySelector('.tp[data-id="' + id + '"]');
    const fresh = rowEl(p);
    if (old && fresh) old.replaceWith(fresh);
    if (focusNote) { const ta = fresh && fresh.querySelector('.note'); if (ta) ta.focus(); }
  }

  function applyFilter(el){
    const s = (get(el.dataset.id).s || '');
    const show = filter === 'all' || (filter === 'open' && !s) || (filter === 'fail' && s === 'F');
    el.classList.toggle('hidden', !show);
  }

  function renderAll(){
    document.querySelectorAll('.tp:not(.tp-head)').forEach(e => e.remove());
    POINTS.forEach(p => {
      const slot = document.querySelector('.tp-slot[data-id="' + p.id + '"]');
      const el = rowEl(p);
      if (slot && el) slot.after(el);
    });
    document.querySelectorAll('.tp-slot').forEach(s => s.remove());
    renderCounts();
  }

  function renderCounts(){
    const tot = { P:0, F:0, NA:0, all: POINTS.length };
    const per = {};
    POINTS.forEach(p => {
      const s = get(p.id).s;
      per[p.group] = per[p.group] || { P:0, F:0, NA:0, all:0 };
      per[p.group].all++;
      if (s) { tot[s]++; per[p.group][s]++; }
    });
    const done = tot.P + tot.F + tot.NA;
    document.getElementById('bar-p').style.width = (100*tot.P/tot.all) + '%';
    document.getElementById('bar-f').style.width = (100*tot.F/tot.all) + '%';
    document.getElementById('bar-n').style.width = (100*tot.NA/tot.all) + '%';
    document.getElementById('st-done').textContent = done + ' / ' + tot.all;
    document.getElementById('st-p').textContent = tot.P;
    document.getElementById('st-f').textContent = tot.F;
    document.getElementById('st-n').textContent = tot.NA;
    document.getElementById('st-open').textContent = tot.all - done;
    GROUPS.forEach(g => {
      const c = per[g] || { P:0, F:0, NA:0, all:0 };
      const d = c.P + c.F + c.NA;
      const nav = document.querySelector('.nav-item[data-group="' + g + '"]');
      if (nav) {
        nav.querySelector('.nav-count').textContent = d + '/' + c.all;
        nav.classList.toggle('done', d === c.all && c.all > 0);
        nav.classList.toggle('hasfail', c.F > 0);
      }
      const gp = document.querySelector('.gprog[data-group="' + g + '"]');
      if (gp) {
        gp.querySelector('.gbar i').style.width = (c.all ? 100*d/c.all : 0) + '%';
        gp.querySelector('.gcount').textContent = d + ' of ' + c.all + (c.F ? ' · ' + c.F + ' failed' : '');
      }
    });
    document.querySelectorAll('.tplist').forEach(l => {
      const rows = l.querySelectorAll('.tp:not(.tp-head)');
      const vis = Array.from(rows).some(r => !r.classList.contains('hidden'));
      l.classList.toggle('allhidden', rows.length > 0 && !vis);
    });
  }

  function escapeHtml(s){ return s.replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }
  function strip(h){ const d = document.createElement('div'); d.innerHTML = h; return d.textContent; }

  // ── run log export ──
  function buildLog(){
    const lines = [];
    const tot = { P:0, F:0, NA:0 };
    POINTS.forEach(p => { const s = get(p.id).s; if (s) tot[s]++; });
    const done = tot.P + tot.F + tot.NA;
    lines.push('# Display test run — ' + new Date().toISOString().slice(0,10));
    lines.push('');
    lines.push('Firmware: Info Display v1.15.1 · Annunciator v3.7.0 · Resource Display v3.14.1');
    lines.push('Result: ' + done + ' of ' + POINTS.length + ' run — ' + tot.P + ' pass, ' + tot.F + ' fail, ' + tot.NA + ' n/a');
    lines.push('');
    const fails = POINTS.filter(p => get(p.id).s === 'F');
    lines.push('## Defects');
    lines.push('');
    if (!fails.length) lines.push('_None recorded._');
    else {
      lines.push('| # | Test point | Observed | Expected |');
      lines.push('|---|---|---|---|');
      fails.forEach((p, i) => lines.push('| D-' + String(i+1).padStart(2,'0') + ' | ' + p.id + ' — ' + strip(p.action).replace(/\|/g,'/') + ' | ' + (get(p.id).note || '').replace(/\n/g,' ').replace(/\|/g,'/') + ' | ' + strip(p.expected).replace(/\|/g,'/') + ' |'));
    }
    const noted = POINTS.filter(p => get(p.id).s !== 'F' && get(p.id).note);
    if (noted.length) {
      lines.push(''); lines.push('## Notes on passing / skipped points'); lines.push('');
      noted.forEach(p => lines.push('- **' + p.id + '** (' + (get(p.id).s || 'open') + '): ' + get(p.id).note.replace(/\n/g,' ')));
    }
    const open = POINTS.filter(p => !get(p.id).s);
    lines.push(''); lines.push('## Not yet run (' + open.length + ')'); lines.push('');
    lines.push(open.map(p => p.id).join(', ') || '_All points run._');
    return lines.join('\n');
  }

  // ── wiring ──
  function init(){
    loadLocal();
    renderAll();
    document.querySelectorAll('#filter button').forEach(b => b.addEventListener('click', () => {
      filter = b.dataset.f;
      document.querySelectorAll('#filter button').forEach(x => x.setAttribute('aria-pressed', x === b));
      document.querySelectorAll('.tp:not(.tp-head)').forEach(applyFilter);
      renderCounts();
    }));
    document.getElementById('export').addEventListener('click', () => {
      const ta = document.getElementById('log');
      ta.value = buildLog();
      ta.hidden = false;
      ta.select();
      if (navigator.clipboard) navigator.clipboard.writeText(ta.value).then(() => { document.getElementById('export-msg').textContent = 'Copied to the clipboard.'; }, () => {});
    });
    document.getElementById('reset').addEventListener('click', () => {
      if (!confirm('Clear every result and note in this run? This cannot be undone.')) return;
      state = { points: {}, updatedAt: null };
      persist();
      renderAll();
    });
    // active nav item on scroll
    const groups = Array.from(document.querySelectorAll('section.group'));
    if ('IntersectionObserver' in window) {
      const io = new IntersectionObserver(entries => {
        entries.forEach(en => {
          if (en.isIntersecting) {
            document.querySelectorAll('.nav-item').forEach(n => n.classList.toggle('active', n.getAttribute('href') === '#' + en.target.id));
          }
        });
      }, { rootMargin: '-80px 0px -70% 0px', threshold: 0 });
      groups.forEach(g => io.observe(g));
    }
    connectDb();
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init); else init();
})();
</script>
"""


def build():
    md = MD.read_text(encoding="utf-8")
    sections = parse(md)
    groups, points = build_model(sections)
    group_ids = [g["id"] for g in groups if not g.get("plain")]

    body = []
    body.append('<title>KCMk1 Display Test Run</title>')
    body.append('<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>')
    body.append('<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@500;600&family=IBM+Plex+Sans:ital,wght@0,400;0,600;1,400&family=IBM+Plex+Mono:wght@400;600&display=swap">')
    body.append(CSS)
    body.append(
        '<div class="topbar"><div class="topbar-in">'
        '<div class="brand"><h1>KCMk1 Display Test Run</h1><small>INFO 1.15.1 · C&amp;W 3.7.0 · RES 3.14.1 · plan v1.0</small></div>'
        '<div class="progress"><div class="pbar" aria-hidden="true"><i class="p" id="bar-p"></i><i class="f" id="bar-f"></i><i class="n" id="bar-n"></i></div>'
        '<div class="pstats"><span><b id="st-done">0</b> run</span>'
        '<span><span class="dot" style="background:var(--pass)"></span><b id="st-p">0</b> pass</span>'
        '<span><span class="dot" style="background:var(--fail)"></span><b id="st-f">0</b> fail</span>'
        '<span><span class="dot" style="background:var(--na)"></span><b id="st-n">0</b> n/a</span>'
        '<span><b id="st-open">0</b> open</span></div></div>'
        '<div class="tools">'
        '<div class="seg" id="filter" role="group" aria-label="Show"><button data-f="all" aria-pressed="true">All</button><button data-f="open" aria-pressed="false">Open</button><button data-f="fail" aria-pressed="false">Failed</button></div>'
        '<a class="btn" href="#run-log">Run log</a>'
        '<button class="btn danger" id="reset" type="button">Clear run</button>'
        '<div class="sync" id="sync"><i class="dot"></i><span>Loading…</span></div>'
        '</div></div></div>'
    )
    body.append('<div class="wrap"><nav class="nav" aria-label="Sessions"><div class="nav-label">Sessions</div>')
    body.append(render_nav(groups))
    body.append('<a class="nav-item plain" href="#run-log"><span class="nav-title">Run log</span></a>')
    body.append('</nav><main>')
    for g in groups:
        if "defect log" in g["title"].lower() or g["id"] == "front-matter":
            continue
        body.append(render_group(g))
    body.append(
        '<section class="group plain" id="run-log"><header class="ghead"><h2><span class="gtitle">Run log</span></h2></header>'
        '<div class="export"><p>Builds a markdown run log from the results above: totals, a defect table from every FAIL with its note, '
        'notes on other points, and the list of points not yet run. Paste it into an issue or the repo.</p>'
        '<div class="row"><button class="btn" id="export" type="button">Build and copy run log</button><span id="export-msg" class="sync"></span></div>'
        '<textarea id="log" hidden aria-label="Run log markdown"></textarea></div></section>'
    )
    body.append('</main></div>')
    body.append('<script>window.__POINTS__=' + json.dumps(points, ensure_ascii=False) + ';window.__GROUPS__=' + json.dumps(group_ids) + ';</script>')
    body.append(JS)
    fragment = "\n".join(body)

    full = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
            '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            '<meta name="color-scheme" content="light dark">\n'
            + fragment.split("\n", 1)[0] + "\n</head>\n<body>\n" + fragment.split("\n", 1)[1] + "\n</body>\n</html>\n")
    OUT.write_text(full, encoding="utf-8")
    print(f"wrote {OUT} ({len(points)} test points, {len(group_ids)} sessions)")
    if "--fragment" in sys.argv:
        fp = Path(sys.argv[sys.argv.index("--fragment") + 1])
        fp.write_text(fragment, encoding="utf-8")
        print(f"wrote {fp}")


if __name__ == "__main__":
    build()
