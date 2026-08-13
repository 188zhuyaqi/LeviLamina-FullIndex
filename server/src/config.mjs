export const config = {
  host: process.env.FULLINDEX_HOST ?? '127.0.0.1',
  port: Number(process.env.FULLINDEX_PORT ?? 30110),
  pluginToken: process.env.FULLINDEX_PLUGIN_TOKEN ?? 'CHANGE_ME',
  webDist: new URL('../../web/dist/', import.meta.url)
}
