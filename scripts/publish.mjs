// Publishes the registry (registry.json) to the library, where PCBKit's app, board library and
// site read it without being rebuilt: every board checked by the pcbkit command-line tool, its
// files packed (but a board opened from GitHub, whose sources are not ours to ship), its picture,
// and the index the gallery shows.
//
//   node scripts/publish.mjs [--dry-run] [--out DIR]
//
// Needs a clean checkout whose HEAD is on origin/main (the published files are that commit's),
// the pcbkit binary (PCBKIT_BIN, default `pcbkit`) and, to upload, the AWS CLI with the right to
// write the library (the dev host's role). It writes:
//   library/demos/<commit>/<slug>.json   a board's files, immutable
//   library/demos/<commit>/<slug>.webp   its picture, immutable
//   library/demos/index.json             the registry with each board's counts and addresses (a minute's cache)
// served at https://library.pcbkit.io/demos/ and, same-origin, app.pcbkit.io/library/demos/.
import { execFileSync } from 'node:child_process'
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync, copyFileSync } from 'node:fs'
import { join, relative } from 'node:path'

const root = new URL('..', import.meta.url).pathname
const args = process.argv.slice(2)
const dry = args.includes('--dry-run')
const out = args.includes('--out') ? args[args.indexOf('--out') + 1] : join(root, '.publish')
const bin = process.env.PCBKIT_BIN ?? 'pcbkit'
const bucket = process.env.PCBKIT_LIBRARY_BUCKET ?? 's3://pcbkit-static-474816328611/library/demos'
const git = (...a) => execFileSync('git', ['-C', root, ...a], { encoding: 'utf8' }).trim()

const registry = JSON.parse(readFileSync(join(root, 'registry.json'), 'utf8'))
// A dry run of a copy that is not a checkout names its files after the working tree.
const commit = (() => { try { return git('rev-parse', 'HEAD') } catch (e) { if (dry) return 'working-tree'; throw e } })()
if (!dry) {
  if (git('status', '--porcelain')) throw new Error('publish: commit everything first (the published files are HEAD’s)')
  git('fetch', '-q', 'origin')
  try { git('merge-base', '--is-ancestor', 'HEAD', 'origin/main') } catch { throw new Error('publish: push HEAD to origin/main first') }
}

// ── The registry checked ──────────────────────────────────────────────────────────────────────
const problems = []
const categories = new Map(registry.categories.map((c) => [c.id, c]))
const seen = new Set()
for (const b of registry.boards) {
  if (seen.has(b.slug)) problems.push(`${b.slug}: twice`)
  seen.add(b.slug)
  if (!categories.has(b.category)) problems.push(`${b.slug}: no category ${b.category}`)
  if (!existsSync(join(root, b.path, '.pcbkit/design.pcbkit'))) problems.push(`${b.slug}: no board at ${b.path}`)
  if (!existsSync(join(root, 'thumbs', `${b.slug}.webp`))) problems.push(`${b.slug}: no picture thumbs/${b.slug}.webp`)
  // An interactive board carries its program (a folder its design names) and is a real board: it
  // can be ordered as it is and says how the ordered board takes that program.
  if (categories.get(b.category)?.interactive && (b.production?.status !== 'ready' || !b.production?.flash)) problems.push(`${b.slug}: interactive, but not ready to order and flash (production)`)
  if (categories.get(b.category)?.interactive && existsSync(join(root, b.path, '.pcbkit/design.pcbkit')) && !/firmware = "/.test(readFileSync(join(root, b.path, '.pcbkit/design.pcbkit'), 'utf8'))) problems.push(`${b.slug}: interactive, but its design names no firmware`)
}
if (problems.length) throw new Error(`publish: the registry has problems:\n  ${problems.join('\n  ')}`)

// ── Every board: checked, counted, packed ─────────────────────────────────────────────────────
/** The text files of a board the app keeps (as PCBKit's workspace does). */
function texts(dir) {
  const found = []
  const walk = (d) => {
    for (const n of readdirSync(d).sort()) {
      const p = join(d, n)
      if (statSync(p).isDirectory()) { if (!['node_modules', 'target', '.git'].includes(n)) walk(p) }
      else if (/\.(pcbkit|md|c|cpp|ino|h|hpp|S|hex|json|js|txt|kicad_pcb|kicad_pro|kicad_dru)$/.test(n)) found.push({ path: relative(dir, p), text: readFileSync(p, 'utf8') })
    }
  }
  walk(dir)
  return found.sort((a, b) => a.path.localeCompare(b.path))
}
const pcbkit = (dir, ...a) => {
  try { return JSON.parse(execFileSync(bin, ['-C', dir, ...a], { encoding: 'utf8', maxBuffer: 1 << 28 })) }
  catch (e) { if (e.stdout) return JSON.parse(e.stdout); throw e }
}

rmSync(out, { recursive: true, force: true })
mkdirSync(join(out, commit), { recursive: true })
const index = []
const warnings = []
for (const b of registry.boards) {
  const dir = join(root, b.path)
  const s = pcbkit(dir, 'call', 'project.summary', '{}').result
  if (s.eval.status !== 'complete' || s.eval.stale || s.eval.warnings) warnings.push(`${b.slug}: ${s.eval.status}${s.eval.stale ? ', copper stale' : ''}, ${s.eval.warnings} warnings`)
  const entry = { ...b, layers: s.layers.length, parts: s.counts.components, nets: s.counts.nets, thumb: `${commit}/${b.slug}.webp` }
  copyFileSync(join(root, 'thumbs', `${b.slug}.webp`), join(out, commit, `${b.slug}.webp`))
  if (b.open === 'github') {
    entry.github = { owner: 'pcbkit-io', repo: 'boards', ref: commit, path: b.path }
    entry.bytes = 0
  } else {
    const json = JSON.stringify({ slug: b.slug, commit, files: texts(dir) })
    writeFileSync(join(out, commit, `${b.slug}.json`), json)
    entry.files = `${commit}/${b.slug}.json`
    entry.bytes = json.length
  }
  index.push(entry)
  console.log(`${b.slug}: ${s.eval.status}, ${entry.parts} parts, ${Math.round(entry.bytes / 1024)} KB`)
}
writeFileSync(join(out, 'index.json'), JSON.stringify({ version: 1, commit, repo: 'https://github.com/pcbkit-io/boards', categories: registry.categories, boards: index }, null, 1))
if (warnings.length) console.warn(`publish: not clean:\n  ${warnings.join('\n  ')}`)

// ── Upload: the commit's files first, then the index that points at them ─────────────────────
if (dry) { console.log(`publish: dry run, ${index.length} boards in ${out}`); process.exit(0) }
for (const [ext, type] of [['json', 'application/json'], ['webp', 'image/webp']])
  execFileSync('aws', ['s3', 'cp', '--recursive', '--quiet', join(out, commit), `${bucket}/${commit}/`, '--exclude', '*', '--include', `*.${ext}`, '--content-type', type, '--cache-control', 'public,max-age=31536000,immutable'], { stdio: 'inherit' })
execFileSync('aws', ['s3', 'cp', '--quiet', join(out, 'index.json'), `${bucket}/index.json`, '--content-type', 'application/json', '--cache-control', 'public,max-age=60'], { stdio: 'inherit' })
console.log(`publish: ${index.length} boards of ${commit.slice(0, 7)} at library.pcbkit.io/demos/`)
