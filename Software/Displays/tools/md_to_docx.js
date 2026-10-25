#!/usr/bin/env node
/*
  md_to_docx.js -- turn one of the Documents/User Markdown references into a Word
  document with real tables and the embedded PNG renders, so the tables can be
  copied straight into the manual.

  Handles the Markdown subset those documents use: # to #### headings, paragraphs
  with **bold**, *italic* and `code`, pipe tables, ![images](relative.png),
  > blockquotes, and - bullet / 1. numbered lists two levels deep. Landscape US Letter so the wide tables fit.

  Usage:
      npm install docx            (once, anywhere on the NODE_PATH or next to this file)
      node tools/md_to_docx.js Documents/User/Annunciator_Caution_Warning_Reference.md [out.docx]
*/
const fs = require("fs");
const path = require("path");
const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell, WidthType, ImageRun,
  HeadingLevel, AlignmentType, ShadingType, BorderStyle, LevelFormat, PageOrientation,
} = require("docx");

const src = process.argv[2];
if (!src) { console.error("usage: md_to_docx.js input.md [output.docx]"); process.exit(2); }
const out = process.argv[3] || src.replace(/\.md$/i, ".docx");
const base = path.dirname(src);
const lines = fs.readFileSync(src, "utf8").split(/\r?\n/);

// Landscape Letter, 0.7" margins -> 9.6" of text width.
const PAGE_W = 15840, PAGE_H = 12240, MARGIN = 1008;
const CONTENT_W = PAGE_W - 2 * MARGIN;          // 13824 DXA
const CONTENT_PX = CONTENT_W / 1440 * 96;        // width in CSS px at 96 dpi

function inline(text, opts = {}) {
  // **bold**, *italic*, `code` -> TextRuns. No nesting needed for these documents.
  const runs = [];
  const re = /(\*\*[^*]+\*\*|\*[^*]+\*|`[^`]+`)/g;
  let last = 0, m;
  const push = (t, extra) => { if (t) runs.push(new TextRun({ text: t, size: opts.size, ...extra })); };
  while ((m = re.exec(text)) !== null) {
    push(text.slice(last, m.index), {});
    const tok = m[0];
    if (tok.startsWith("**")) push(tok.slice(2, -2), { bold: true });
    else if (tok.startsWith("`")) push(tok.slice(1, -1), { font: "Consolas" });
    else push(tok.slice(1, -1), { italics: true });
    last = m.index + tok.length;
  }
  push(text.slice(last), {});
  return runs;
}

function pngSize(file) {
  const b = fs.readFileSync(file);
  return { w: b.readUInt32BE(16), h: b.readUInt32BE(20), data: b };
}

function imagePara(rel, alt) {
  const file = path.join(base, rel);
  if (!fs.existsSync(file)) return new Paragraph({ children: [new TextRun({ text: `[missing image ${rel}]`, italics: true })] });
  const { w, h, data } = pngSize(file);
  const scale = Math.min(1, CONTENT_PX / w);
  return new Paragraph({
    alignment: AlignmentType.CENTER, spacing: { before: 120, after: 60 },
    children: [new ImageRun({ type: "png", data, altText: { title: alt, description: alt, name: alt },
                              transformation: { width: Math.round(w * scale), height: Math.round(h * scale) } })],
  });
}

function splitRow(line) {
  return line.trim().replace(/^\||\|$/g, "").split("|").map(c => c.trim());
}

function table(rows) {
  const header = splitRow(rows[0]);
  const body = rows.slice(2).map(splitRow);
  const n = header.length;
  // Column widths in proportion to the longest cell in each column, floored so short
  // columns stay readable.
  const maxLen = header.map((h, i) => Math.max(h.length, ...body.map(r => (r[i] || "").length)));
  const weights = maxLen.map(l => Math.max(6, Math.min(l, 60)));
  const total = weights.reduce((a, b) => a + b, 0);
  const widths = weights.map(w => Math.round(CONTENT_W * w / total));
  widths[n - 1] += CONTENT_W - widths.reduce((a, b) => a + b, 0);
  const border = { style: BorderStyle.SINGLE, size: 4, color: "999999" };
  const borders = { top: border, bottom: border, left: border, right: border };
  const cell = (text, i, isHeader) => new TableCell({
    width: { size: widths[i], type: WidthType.DXA }, borders,
    shading: isHeader ? { type: ShadingType.CLEAR, fill: "E7E6E6", color: "auto" } : undefined,
    margins: { top: 40, bottom: 40, left: 80, right: 80 },
    children: [new Paragraph({ children: isHeader
      ? [new TextRun({ text: text.replace(/\*\*/g, ""), bold: true, size: 17 })]
      : inline(text, { size: 17 }) })],
  });
  return new Table({
    width: { size: CONTENT_W, type: WidthType.DXA }, columnWidths: widths,
    rows: [
      new TableRow({ tableHeader: true, children: header.map((h, i) => cell(h, i, true)) }),
      ...body.map(r => new TableRow({ children: header.map((_, i) => cell(r[i] || "", i, false)) })),
    ],
  });
}

const children = [];
let i = 0;
let listInstance = 0;
while (i < lines.length) {
  const line = lines[i];
  if (/^\s*$/.test(line)) { i++; continue; }
  let m;
  if ((m = line.match(/^(#{1,4})\s+(.*)$/))) {
    const lvl = [HeadingLevel.HEADING_1, HeadingLevel.HEADING_2, HeadingLevel.HEADING_3, HeadingLevel.HEADING_4][m[1].length - 1];
    children.push(new Paragraph({ heading: lvl, spacing: { before: 240, after: 120 }, children: inline(m[2]) }));
    i++; continue;
  }
  if ((m = line.match(/^!\[([^\]]*)\]\(([^)]+)\)\s*$/))) { children.push(imagePara(m[2], m[1])); i++; continue; }
  if (line.startsWith("|")) {
    const rows = [];
    while (i < lines.length && lines[i].startsWith("|")) rows.push(lines[i++]);
    children.push(table(rows));
    children.push(new Paragraph({ spacing: { after: 120 }, children: [] }));
    continue;
  }
  if (line.startsWith(">")) {
    const parts = [];
    while (i < lines.length && lines[i].startsWith(">")) parts.push(lines[i++].replace(/^>\s?/, ""));
    children.push(new Paragraph({ indent: { left: 567 }, spacing: { after: 120 },
      border: { left: { style: BorderStyle.SINGLE, size: 12, color: "999999", space: 8 } },
      children: inline(parts.join(" ").replace(/\s+/g, " ")) }));
    continue;
  }
  // Lists: "- " bullets and "1. " numbered items, at column 0 (level 0) or indented
  // 2-4 spaces (level 1). A deeper-indented line that is not an item continues the
  // previous item. Each run of numbered items restarts at 1.
  const ITEM = /^( {0,4})(- |\d+\. )(.*)$/;
  if ((m = line.match(ITEM)) && m[1].length === 0) {
    const inst = ++listInstance;
    while (i < lines.length) {
      const im = lines[i].match(ITEM);
      if (!im) break;
      const level = im[1].length >= 2 ? 1 : 0;
      const numbered = /^\d/.test(im[2]);
      let text = im[3]; i++;
      while (i < lines.length && !ITEM.test(lines[i]) && /^\s{2,}\S/.test(lines[i])) text += " " + lines[i++].trim();
      const numbering = numbered ? { reference: "numbers", level, instance: inst } : { reference: "bullets", level };
      children.push(new Paragraph({ numbering, spacing: { after: 60 }, children: inline(text) }));
      while (i < lines.length && /^\s*$/.test(lines[i]) && i + 1 < lines.length && ITEM.test(lines[i + 1]) && lines[i + 1].match(ITEM)[1].length >= 2) i++;
    }
    continue;
  }
  // Paragraph: consecutive non-blank lines; a line ending in two spaces forces a break.
  const parts = [];
  while (i < lines.length && !/^\s*$/.test(lines[i]) && !/^(#|\||>|- |\d+\. |!\[)/.test(lines[i])) {
    const l = lines[i++];
    parts.push(l);
    if (/ {2}$/.test(l)) break;
  }
  const text = parts.join(" ").replace(/\s+/g, " ").trim();
  const italicOnly = /^\*[^*].*\*$/.test(text) && !text.slice(1, -1).includes("*");
  children.push(new Paragraph({ spacing: { after: 120 },
    children: italicOnly ? [new TextRun({ text: text.slice(1, -1), italics: true })] : inline(text) }));
}

const doc = new Document({
  numbering: { config: [
    { reference: "bullets", levels: [
      { level: 0, format: LevelFormat.BULLET, text: "\u2022", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 567, hanging: 283 } } } },
      { level: 1, format: LevelFormat.BULLET, text: "\u2013", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 1134, hanging: 283 } } } } ] },
    { reference: "numbers", levels: [
      { level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 567, hanging: 340 } } } },
      { level: 1, format: LevelFormat.LOWER_LETTER, text: "%2.", alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 1134, hanging: 340 } } } } ] },
  ] },
  styles: { default: { document: { run: { font: "Calibri", size: 21 } } } },
  sections: [{
    properties: { page: { size: { width: PAGE_H, height: PAGE_W, orientation: PageOrientation.LANDSCAPE },
                          margin: { top: MARGIN, bottom: MARGIN, left: MARGIN, right: MARGIN } } },
    children,
  }],
});

Packer.toBuffer(doc).then(buf => { fs.writeFileSync(out, buf); console.log("wrote", out, buf.length, "bytes"); });
