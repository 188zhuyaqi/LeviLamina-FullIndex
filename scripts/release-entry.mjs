import { fileURLToPath } from 'node:url'

process.env.FULLINDEX_RELEASE_ROOT ??= fileURLToPath(new URL('.', import.meta.url))

await import('../server/src/index.mjs')
