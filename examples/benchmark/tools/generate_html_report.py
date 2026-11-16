#!/usr/bin/env python3
"""Generate a self-contained file:// compatible benchmark report."""
import argparse
import html
import json
import shutil
from pathlib import Path

STYLE = """
:root{--ink:#172033;--muted:#60708a;--line:#dbe2ea;--bg:#f5f7fa}*{box-sizing:border-box}body{margin:0;font:15px/1.5 Segoe UI,Arial,sans-serif;color:var(--ink);background:var(--bg)}header{padding:52px max(5vw,28px);color:white;background:linear-gradient(120deg,#102649,#1769e0 62%,#00a495)}header h1{margin:0 0 8px;font-size:clamp(30px,5vw,58px);line-height:1.05}.wrap{max-width:1280px;margin:auto;padding:28px}section{background:white;border:1px solid var(--line);border-radius:14px;margin:18px 0;padding:24px;box-shadow:0 3px 18px #1026490c}h2{margin-top:0}.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:14px}.card{border:1px solid var(--line);border-radius:11px;padding:18px}.metric{font-size:32px;font-weight:750}.muted{color:var(--muted)}.disclosure{border-left:5px solid #f0a202;background:#fff8e6;padding:14px 18px}.controls{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:14px}input,select{padding:9px 11px;border:1px solid #bcc8d8;border-radius:7px;background:white}table{width:100%;border-collapse:collapse;font-size:13px}th,td{padding:9px;border-bottom:1px solid var(--line);text-align:left;vertical-align:top}th{position:sticky;top:0;background:#edf3fa}.scroll{overflow:auto;max-height:650px}.correct{color:#087f5b}.not_found,.wrong_text,.wrong_format{color:#c2410c}.unsupported_format{color:#6d28d9}.decoder_error{color:#b91c1c;font-weight:600}td.mono,th.mono{font-family:Consolas,Menlo,monospace;font-size:12.5px;word-break:break-all}#fail{table-layout:fixed}#fail th:nth-child(1){width:132px}#fail th:nth-child(2){width:186px}#fail th:nth-child(3){width:100px}#fail th:nth-child(4),#fail th:nth-child(5){width:252px}#fail th:nth-child(6){width:118px}#fail th:nth-child(7){width:48px}#fail td:nth-child(2),#fail td:nth-child(4),#fail td:nth-child(5){white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.filters{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:10px;margin:14px 0}.tools{display:flex;gap:10px;align-items:center;margin:10px 0}.tools button{padding:8px 12px;border:1px solid #bcc8d8;border-radius:7px;background:white;cursor:pointer}@media print{body{background:white}.controls{display:none}section{break-inside:avoid;box-shadow:none}}
"""

SCRIPT = """
const rows=REPORT_ROWS,body=document.querySelector('#raw tbody');
const esc=v=>String(v).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function render(){const q=document.querySelector('#q').value.toLowerCase(),d=document.querySelector('#decoder').value,o=document.querySelector('#outcome').value;body.innerHTML='';const matched=rows.filter(r=>(!d||r.decoder===d)&&(!o||r.matches.some(m=>m.outcome===o))&&JSON.stringify(r).toLowerCase().includes(q));document.querySelector('#shown').textContent=`Showing ${Math.min(500,matched.length).toLocaleString()} of ${matched.length.toLocaleString()} matching records`;matched.slice(0,500).forEach(r=>{const tr=document.createElement('tr'),out=r.matches.filter(m=>m.outcome!=='extra_result').map(m=>m.outcome);tr.innerHTML=`<td>${esc(r.decoder)}</td><td>${esc(r.relative_path)}</td><td>${esc(r.annotation_file)}</td><td>${r.ground_truth.filter(x=>x.decode_eligible).map(x=>esc(x.format)+': '+esc(x.text)).join('<br>')}</td><td>${r.predictions.map(x=>esc(x.format)+': '+esc(x.text)).join('<br>')}</td><td class="${out[0]||''}">${out.map(esc).join(', ')}</td><td>${(r.decode_ns/1e6).toFixed(2)}</td>`;body.appendChild(tr)})}
document.querySelectorAll('#q,#decoder,#outcome').forEach(e=>e.addEventListener('input',render));render();

// --- failures to follow up -------------------------------------------------
const fails=[];
for(const r of rows){
  for(const m of r.matches){
    if(m.outcome==='correct'||m.outcome==='extra_result')continue;
    const gt=r.ground_truth[m.truth_index]||{};
    const pr=(m.prediction_index!=null&&r.predictions[m.prediction_index])?r.predictions[m.prediction_index]:null;
    fails.push({d:r.decoder,img:r.relative_path,src:r.annotation_file,fmt:gt.format||'',exp:gt.text||'',got:pr?(pr.format+': '+pr.text):'',out:m.outcome,ms:Math.round(r.decode_ns/1e6)});
  }
}
const RANK={wrong_text:0,wrong_format:1,unsupported_format:2,not_found:3,decoder_error:4};
fails.sort((a,b)=>a.d.localeCompare(b.d)||(RANK[a.out]-RANK[b.out])||a.fmt.localeCompare(b.fmt)||a.img.localeCompare(b.img));
const fbody=document.querySelector('#fail tbody'),fcount=document.querySelector('#failcount');
function renderFail(){
  const q=document.querySelector('#fq').value.toLowerCase(),d=document.querySelector('#fdec').value,o=document.querySelector('#fout').value,f=document.querySelector('#ffmt').value;
  const m=fails.filter(x=>(!d||x.d===d)&&(!o||x.out===o)&&(!f||x.fmt===f)&&(!q||(x.img+' '+x.fmt+' '+x.exp+' '+x.got+' '+x.src).toLowerCase().includes(q)));
  const byOut={};for(const x of m)byOut[x.out]=(byOut[x.out]||0)+1;
  fcount.textContent=m.length.toLocaleString()+' failures'+(m.length?(' - '+Object.keys(byOut).sort().map(k=>k+' '+byOut[k]).join(', ')):'');
  fbody.innerHTML='';
  const cap=3000;
  for(const x of m.slice(0,cap)){
    const tr=document.createElement('tr');
    tr.innerHTML='<td>'+esc(x.d)+'</td><td class="mono" title="'+esc(x.src)+'">'+esc(x.img)+'</td><td>'+esc(x.fmt)+'</td>'+
      '<td class="mono" title="'+esc(x.exp)+'">'+esc(x.exp)+'</td>'+
      '<td class="mono" title="'+esc(x.got)+'">'+(x.got?esc(x.got):'<span class="muted">no read</span>')+'</td>'+
      '<td class="'+esc(x.out)+'">'+esc(x.out)+'</td><td>'+x.ms+'</td>';
    fbody.appendChild(tr);
  }
  if(m.length>cap){const tr=document.createElement('tr');tr.innerHTML='<td colspan="7" class="muted">'+(m.length-cap).toLocaleString()+' more rows - narrow the filters or use the JSONL.</td>';fbody.appendChild(tr);}
}
document.querySelectorAll('#fq,#fdec,#fout,#ffmt').forEach(e=>e.addEventListener('input',renderFail));
const fdec=document.querySelector('#fdec');
for(const d of Array.from(new Set(fails.map(x=>x.d))).sort()){const o=document.createElement('option');o.textContent=d;fdec.appendChild(o);}
const ffmt=document.querySelector('#ffmt');
for(const f of Array.from(new Set(fails.map(x=>x.fmt))).sort()){const o=document.createElement('option');o.textContent=f;ffmt.appendChild(o);}
renderFail();
function csvCell(v){const s=String(v==null?'':v);const needs=s.indexOf('"')>=0||s.indexOf(',')>=0||s.indexOf(String.fromCharCode(10))>=0;return needs?'"'+s.split('"').join('""')+'"':s;}
document.querySelector('#fcsv').addEventListener('click',()=>{
  const out=[['decoder','image','source','format','expected','returned','outcome','decode_ms']];
  const d=document.querySelector('#fdec').value,o=document.querySelector('#fout').value,f=document.querySelector('#ffmt').value,q=document.querySelector('#fq').value.toLowerCase();
  for(const x of fails){if((d&&x.d!==d)||(o&&x.out!==o)||(f&&x.fmt!==f)||(q&&!(x.img+' '+x.fmt+' '+x.exp+' '+x.got+' '+x.src).toLowerCase().includes(q)))continue;out.push([x.d,x.img,x.src,x.fmt,x.exp,x.got,x.out,x.ms]);}
  const csv=out.map(r=>r.map(csvCell).join(',')).join(String.fromCharCode(10));
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));a.download='barber-failures.csv';a.click();
  setTimeout(()=>URL.revokeObjectURL(a.href),2000);
});
"""

DECODER_LABELS = {
    "dynamsoft-dbr": "Dynamsoft Barcode Reader",
    "scandit-linux": "Scandit Barcode Scanner SDK (Linux)",
    "zxing-cpp": "ZXing-C++",
}
DECODER_ORDER = ["dynamsoft-dbr", "scandit-linux", "zxing-cpp"]


def ordered_decoders(summary_decoders):
    names = list(summary_decoders)
    return sorted(names, key=lambda n: (DECODER_ORDER.index(n) if n in DECODER_ORDER else 99, n))


def label(name):
    return DECODER_LABELS.get(name, name)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", type=Path, required=True)
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--results-json", type=Path)
    parser.add_argument("--matching-analysis", type=Path)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--environment", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    with args.results.open(encoding="utf8") as source:
        rows = [json.loads(line) for line in source if line.strip()]
    summary = json.loads(args.summary.read_text(encoding="utf8"))
    stats = json.loads(args.inventory.read_text(encoding="utf8"))["summary"]
    environment = json.loads(args.environment.read_text(encoding="utf8"))

    names = ordered_decoders(summary["decoders"])
    cards = "".join(
        f'<div class="card"><h3>{html.escape(label(name))}</h3>'
        f'<div class="metric">{summary["decoders"][name]["coverage_adjusted_recall"]:.1%}</div>'
        f'<div class="muted">{summary["decoders"][name]["correct"]:,}/{summary["decoders"][name]["eligible_instances"]:,} exact'
        f' &middot; {summary["decoders"][name]["wrong_reads"]:,} wrong reads'
        f' &middot; {summary["decoders"][name]["no_read"]:,} no read'
        f' &middot; mean {summary["decoders"][name]["mean_decode_ms"]:.1f} ms</div></div>'
        for name in names
    )
    comparison_rows = "".join(
        f'<tr><td><strong>{html.escape(label(name))}</strong><br><span class="muted">{html.escape(name)}</span></td>'
        f'<td>{v["correct"]:,} / {v["eligible_instances"]:,}</td>'
        f'<td>{v["coverage_adjusted_recall"]:.2%}</td>'
        f'<td>{v["wrong_reads"]:,}</td>'
        f'<td>{v["no_read"]:,}</td>'
        f'<td>{v["unsupported"]:,}</td>'
        f'<td>{v["mean_decode_ms"]:.2f}</td>'
        f'<td>{v["median_decode_ms"]:.2f}</td>'
        f'<td>{v["p95_decode_ms"]:.2f}</td></tr>'
        for name in names for v in [summary["decoders"][name]]
    )
    environment_rows = "".join(
        f'<tr><td>{html.escape(str(k))}</td><td>{html.escape(str(value))}</td></tr>'
        for k, value in environment.items()
    )
    decoder_options = "".join(f'<option>{html.escape(x)}</option>' for x in sorted({r["decoder"] for r in rows}))
    outcomes = sorted({m["outcome"] for r in rows for m in r["matches"]} - {"extra_result"})
    outcome_options = "".join(f'<option>{html.escape(x)}</option>' for x in outcomes)
    script = SCRIPT.replace("REPORT_ROWS", json.dumps(rows, ensure_ascii=False).replace("</", "<\\/"))
    decoder_records = sum(value["records"] for value in summary["decoders"].values())
    title = html.escape(summary.get("title", "BarBeR Barcode Benchmark"))
    reader_count = len(names)
    reader_word = "Both decoders" if reader_count == 2 else f"All {reader_count} readers"

    document = f'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>BarBeR Barcode Benchmark</title><style>{STYLE}</style><header><h1>{title}</h1><p>BarBeR public dataset decoding benchmark. Exact payload and canonical format are scored. Localization geometry is not scored.</p></header><main class="wrap"><section><h2>Full benchmark results</h2><div class="cards">{cards}</div></section><section><h2>Decoder comparison</h2><p class="muted">Recall is correct ground truth matches divided by scored ground truth instances. Every annotation resolves to correct, wrong read or no read, and the three sum to the scored total; a prediction that no annotation claims is set aside rather than charged to an instance, because BarBeR's annotations are not exhaustive. Unsupported counts show how much of the no-read column is a symbology the reader does not implement. Decode time covers the decoder call only; image loading, matching and serialization are excluded.</p><div class="scroll"><table><thead><tr><th>Decoder</th><th>Correct</th><th>Recall</th><th>Wrong reads</th><th>No read</th><th>Unsupported</th><th>Mean ms</th><th>Median ms</th><th>P95 ms</th></tr></thead><tbody>{comparison_rows}</tbody></table></div></section><section><h2>Dataset audit</h2><div class="cards"><div class="card"><div class="metric">{stats['source_images']:,}</div>original images</div><div class="card"><div class="metric">{stats['annotations']:,}</div>original annotations</div><div class="card"><div class="metric">{stats['excluded_images_without_ground_truth']:,}</div>images without reliable ground truth</div><div class="card"><div class="metric">{stats['duplicate_image_records']:,}</div>exact duplicate image</div><div class="card"><div class="metric">{stats['manifest_images']:,}</div>final unique images</div><div class="card"><div class="metric">{stats['benchmark_annotations']:,}</div>final ground truth barcodes</div><div class="card"><div class="metric">{decoder_records:,}</div>decoder records</div></div><p class="muted">The consistency check links 8,748 original images and 9,818 original annotations to 7,894 tested images, 8,615 audited ground truth values, 7,894 unique SHA-256 image hashes, and one record per decoder for each tested image. 204 audited payloads are the unreliable placeholder ^ and are excluded from scoring, leaving 8,411 scored ground truth values.</p></section><section><h2>Method and disclosures</h2><p class="disclosure">{html.escape(summary['disclosure'])}</p><p>{reader_word} receive the same RGB888 pixel buffer loaded once per image. All supported formats are enabled with no per-image format or barcode-count hint. Matching is a location-independent one-to-one multiset match of canonical format and exact normalized payload. UPC-A/EAN-13 leading zeros, CODE_39 start/stop asterisks, CODE_128 GS1 markers, HTML entity escaping, trailing newlines, and a leading \\000001 escape are normalized before scoring. Ground truth payload ^ is treated as an unreliable placeholder and excluded from scoring.</p><p><strong>Measured environment:</strong> {html.escape(environment['operating_system'])}, {html.escape(environment['processor'])}, {environment['memory_gb']} GB memory, {html.escape(environment['configuration'])} {html.escape(environment['architecture'])}, {environment['threads_per_decoder_task']} thread per decoder task, {environment['repetitions']} measured run.</p><div class="scroll"><table><thead><tr><th>Environment key</th><th>Value</th></tr></thead><tbody>{environment_rows}</tbody></table></div></section><section><h2>Per-image results</h2><p class="muted">The interactive table displays up to 500 matching rows. The Predictions column lists every barcode the reader returned for the image, including any that no annotation claims; the Outcomes column covers only the annotations. The complete JSONL stream, the summary, the source inventory and the environment snapshot are shipped under <code>results/full_linux/</code>, <code>manifests/</code> and <code>configs/</code>.</p><div class="controls"><input id="q" placeholder="Search image, format, payload"><select id="decoder"><option value="">All decoders</option>{decoder_options}</select><select id="outcome"><option value="">All outcomes</option>{outcome_options}</select></div><p id="shown" class="muted"></p><div class="scroll"><table id="raw"><thead><tr><th>Decoder</th><th>Image</th><th>Source</th><th>Ground truth</th><th>Predictions</th><th>Outcomes</th><th>Decode ms</th></tr></thead><tbody></tbody></table></div></section><section id="failures"><h2>Failures to follow up</h2><p class="muted">Every annotation that did not end as <code>correct</code>, one row per annotation: the image, the expected symbology and payload, what the reader returned for that annotation, and the outcome. Rows are grouped by decoder, then outcome (wrong reads first). The source annotation file is in the cell tooltip and in the CSV export. Use the filters to isolate one reader, one outcome or one format; the underlying records are in <code>results/full_linux/results.jsonl</code>.</p><div class="filters"><input id="fq" placeholder="Search image, payload, source"><select id="fdec"><option value="">All decoders</option></select><select id="fout"><option value="">All outcomes</option><option>wrong_text</option><option>wrong_format</option><option>unsupported_format</option><option>not_found</option><option>decoder_error</option></select><select id="ffmt"><option value="">All formats</option></select></div><div class="tools"><button id="fcsv" type="button">Download filtered rows as CSV</button></div><p id="failcount" class="muted"></p><div class="scroll"><table id="fail"><thead><tr><th>Decoder</th><th>Image</th><th>Format</th><th>Expected</th><th>Returned</th><th>Outcome</th><th>ms</th></tr></thead><tbody></tbody></table></div></section></main><script>{script}</script></html>'''
    document = document.replace("<title>", '<link rel="icon" href="data:,\"><title>', 1)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(document, encoding="utf8")
    downloads = args.output.parent / "downloads"
    downloads.mkdir(exist_ok=True)
    downloads_list = [(args.results, "results.jsonl"), (args.summary, "summary.json"), (args.inventory, "barber_source_files.json"), (args.environment, "benchmark_environment.json")]
    if args.results_json:
        downloads_list.append((args.results_json, "results.json"))
    if args.matching_analysis:
        downloads_list.append((args.matching_analysis, "matching_analysis.json"))
    for source, name in downloads_list:
        shutil.copy2(source, downloads / name)
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
