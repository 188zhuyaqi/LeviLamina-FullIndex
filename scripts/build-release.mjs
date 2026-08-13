import { execFileSync } from 'node:child_process'
import fs from 'node:fs'
import path from 'node:path'
import process from 'node:process'
import { fileURLToPath } from 'node:url'

import { build } from 'esbuild'

const projectRoot = path.resolve(fileURLToPath(new URL('..', import.meta.url)))
const releaseDir = path.resolve(projectRoot, 'release')
const webDist = path.resolve(projectRoot, 'web/dist')

if (path.dirname(releaseDir) !== projectRoot || path.basename(releaseDir) !== 'release') {
  throw new Error(`refusing to replace unexpected release path: ${releaseDir}`)
}

const viteEntry = path.join(projectRoot, 'node_modules/vite/bin/vite.js')
execFileSync(process.execPath, [viteEntry, 'build'], {
  cwd: path.join(projectRoot, 'web'),
  stdio: 'inherit'
})

fs.mkdirSync(releaseDir, { recursive: true })
for (const entry of fs.readdirSync(releaseDir)) {
  fs.rmSync(path.join(releaseDir, entry), { recursive: true, force: true })
}
fs.mkdirSync(path.join(releaseDir, 'data'), { recursive: true })
fs.cpSync(webDist, path.join(releaseDir, 'web'), { recursive: true })

await build({
  entryPoints: [path.join(projectRoot, 'scripts/release-entry.mjs')],
  outfile: path.join(releaseDir, 'main.js'),
  bundle: true,
  platform: 'node',
  format: 'esm',
  target: 'node24',
  minify: true,
  sourcemap: false,
  banner: {
    js: "/* FullIndex production gateway */\nimport { createRequire } from 'node:module';\nconst require = createRequire(import.meta.url);"
  }
})

fs.writeFileSync(path.join(releaseDir, 'package.json'), `${JSON.stringify({
  name: 'fullindex-release',
  private: true,
  type: 'module',
  engines: { node: '>=24' }
}, null, 2)}\n`, 'utf8')

fs.writeFileSync(path.join(releaseDir, 'start.cmd'), '@echo off\r\nnode main.js\r\n', 'utf8')
fs.writeFileSync(path.join(releaseDir, 'README.txt'), [
  'FullIndex Web / Node production package',
  '',
  'Requires Node.js 24 or newer.',
  'Start: node main.js',
  'Open: http://127.0.0.1:30110',
  '',
  'Optional environment variables:',
  'FULLINDEX_HOST, FULLINDEX_PORT, FULLINDEX_PLUGIN_TOKEN,',
  'FULLINDEX_INDEX_DB, FULLINDEX_KEEP_SNAPSHOTS',
  '',
  'The SQLite database is created under data/ by default.'
].join('\r\n'), 'utf8')

const files = fs.readdirSync(releaseDir, { recursive: true })
console.log(`\nRelease created: ${releaseDir}`)
console.log(`Files: ${files.length}`)
