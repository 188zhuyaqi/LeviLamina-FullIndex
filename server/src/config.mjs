import { fileURLToPath } from 'node:url'
import path from 'node:path'

const releaseRoot = process.env.FULLINDEX_RELEASE_ROOT
const releasePath = name => path.join(releaseRoot, name)

export const config = {
  host: process.env.FULLINDEX_HOST ?? '127.0.0.1',
  port: Number(process.env.FULLINDEX_PORT ?? 30110),
  pluginToken: process.env.FULLINDEX_PLUGIN_TOKEN ?? 'CHANGE_ME',
  webDist: releaseRoot ? releasePath('web') : new URL('../../web/dist/', import.meta.url),
  indexDatabase: process.env.FULLINDEX_INDEX_DB
    ?? (releaseRoot ? releasePath('data/fullindex.sqlite3')
      : fileURLToPath(new URL('../data/fullindex.sqlite3', import.meta.url))),
  keepSnapshots: Number(process.env.FULLINDEX_KEEP_SNAPSHOTS ?? 10)
}
