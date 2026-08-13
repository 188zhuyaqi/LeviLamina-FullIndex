import fs from 'node:fs'
import path from 'node:path'
import { DatabaseSync } from 'node:sqlite'

function walkItems(items, context, visit, prefix = '') {
  for (const item of items ?? []) {
    const pathName = prefix ? `${prefix}/${item.slotName || item.slot}` : String(item.slotName || item.slot)
    visit(item, context, pathName)
    walkItems(item.children, context, visit, `${pathName}:${item.id}`)
  }
}

function firstPosition(positions) {
  return Array.isArray(positions) && positions.length > 0 ? positions[0] : {}
}

function uniqueStrings(values) {
  return [...new Set((values ?? []).map(value => String(value ?? '').trim()).filter(Boolean))]
}

function normalizedPlayerAliases(player) {
  return uniqueStrings([player?.xuid, player?.uuid, ...(player?.storageIds ?? [])])
    .map(value => value.toLowerCase())
}

function displayPlayerName(player) {
  return player.realName || player.xuid || player.uuid || player.storageIds?.[0] || 'unknown-player'
}

function mergePlayerRecord(current, incoming) {
  if (!current) return { ...incoming, storageIds: uniqueStrings(incoming.storageIds) }
  const storageIds = uniqueStrings([...(current.storageIds ?? []), ...(incoming.storageIds ?? [])]).sort()
  const realName = incoming.realName || current.realName || ''
  const xuid = incoming.xuid || current.xuid || ''
  const uuid = incoming.uuid || current.uuid || ''
  const merged = incoming.online && !current.online ? { ...incoming } : { ...current }
  merged.online = Boolean(current.online || incoming.online)
  merged.realName = realName
  merged.xuid = xuid
  merged.uuid = uuid
  merged.storageIds = storageIds
  merged.name = displayPlayerName(merged)
  return merged
}

export class IndexStore {
  constructor(filename, keepSnapshots = 10) {
    fs.mkdirSync(path.dirname(filename), { recursive: true })
    this.db = new DatabaseSync(filename)
    this.keepSnapshots = keepSnapshots
    this.db.exec(`
      PRAGMA journal_mode = WAL;
      PRAGMA foreign_keys = ON;
      CREATE TABLE IF NOT EXISTS snapshots (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        server_id TEXT NOT NULL,
        created_at INTEGER NOT NULL,
        player_count INTEGER NOT NULL,
        container_count INTEGER NOT NULL,
        drop_chunk_count INTEGER NOT NULL,
        entity_count INTEGER NOT NULL,
        item_record_count INTEGER NOT NULL DEFAULT 0
      );
      CREATE INDEX IF NOT EXISTS snapshots_server_created
        ON snapshots(server_id, created_at DESC);

      CREATE TABLE IF NOT EXISTS players (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        snapshot_id INTEGER NOT NULL REFERENCES snapshots(id) ON DELETE CASCADE,
        source TEXT NOT NULL, name TEXT NOT NULL, real_name TEXT NOT NULL,
        xuid TEXT NOT NULL, uuid TEXT NOT NULL, aliases_text TEXT NOT NULL,
        online INTEGER NOT NULL, dimension TEXT NOT NULL,
        x REAL, y REAL, z REAL, detail_json TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS players_snapshot_name ON players(snapshot_id, name);
      CREATE INDEX IF NOT EXISTS players_snapshot_online ON players(snapshot_id, online);
      CREATE INDEX IF NOT EXISTS players_snapshot_dimension ON players(snapshot_id, dimension);

      CREATE TABLE IF NOT EXISTS containers (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        snapshot_id INTEGER NOT NULL REFERENCES snapshots(id) ON DELETE CASCADE,
        source TEXT NOT NULL, kind TEXT NOT NULL, dimension TEXT NOT NULL,
        chunk_x INTEGER NOT NULL, chunk_z INTEGER NOT NULL,
        x REAL NOT NULL, y REAL NOT NULL, z REAL NOT NULL,
        occupied_slots INTEGER NOT NULL, item_count INTEGER NOT NULL,
        detail_json TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS containers_snapshot_kind ON containers(snapshot_id, kind);
      CREATE INDEX IF NOT EXISTS containers_snapshot_chunk ON containers(snapshot_id, dimension, chunk_x, chunk_z);
      CREATE INDEX IF NOT EXISTS containers_snapshot_position ON containers(snapshot_id, dimension, x, y, z);

      CREATE TABLE IF NOT EXISTS drops (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        snapshot_id INTEGER NOT NULL REFERENCES snapshots(id) ON DELETE CASCADE,
        source TEXT NOT NULL, dimension TEXT NOT NULL,
        chunk_x INTEGER NOT NULL, chunk_z INTEGER NOT NULL,
        entity_count INTEGER NOT NULL, item_count INTEGER NOT NULL,
        distinct_item_count INTEGER NOT NULL, detail_json TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS drops_snapshot_chunk ON drops(snapshot_id, dimension, chunk_x, chunk_z);

      CREATE TABLE IF NOT EXISTS entities (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        snapshot_id INTEGER NOT NULL REFERENCES snapshots(id) ON DELETE CASCADE,
        source TEXT NOT NULL, type_name TEXT NOT NULL, custom_name TEXT NOT NULL,
        category TEXT NOT NULL, dimension TEXT NOT NULL,
        chunk_x INTEGER NOT NULL, chunk_z INTEGER NOT NULL,
        x REAL NOT NULL, y REAL NOT NULL, z REAL NOT NULL,
        detail_json TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS entities_snapshot_type ON entities(snapshot_id, type_name);
      CREATE INDEX IF NOT EXISTS entities_snapshot_category ON entities(snapshot_id, category);
      CREATE INDEX IF NOT EXISTS entities_snapshot_chunk ON entities(snapshot_id, dimension, chunk_x, chunk_z);

      CREATE TABLE IF NOT EXISTS items (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        snapshot_id INTEGER NOT NULL REFERENCES snapshots(id) ON DELETE CASCADE,
        source_type TEXT NOT NULL,
        owner TEXT NOT NULL,
        item_path TEXT NOT NULL,
        item_id TEXT NOT NULL,
        display_name TEXT NOT NULL,
        count INTEGER NOT NULL,
        dimension TEXT NOT NULL,
        x REAL,
        y REAL,
        z REAL,
        chunk_x INTEGER,
        chunk_z INTEGER,
        enchanted INTEGER NOT NULL,
        has_container_data INTEGER NOT NULL,
        custom_name TEXT NOT NULL,
        damage INTEGER NOT NULL,
        owner_record_id INTEGER,
        detail_json TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS items_snapshot_item
        ON items(snapshot_id, item_id);
      CREATE INDEX IF NOT EXISTS items_snapshot_source
        ON items(snapshot_id, source_type);
      CREATE INDEX IF NOT EXISTS items_snapshot_owner_record
        ON items(snapshot_id, source_type, owner_record_id);
      CREATE INDEX IF NOT EXISTS items_snapshot_name
        ON items(snapshot_id, display_name, custom_name);

      CREATE TABLE IF NOT EXISTS player_identity_maps (
        server_id TEXT PRIMARY KEY,
        updated_at INTEGER NOT NULL,
        payload TEXT NOT NULL
      );
    `)

    this.identityMaps = new Map()

    this.insertSnapshot = this.db.prepare(`
      INSERT INTO snapshots (
        server_id, created_at, player_count, container_count,
        drop_chunk_count, entity_count, item_record_count
      ) VALUES (?, ?, ?, ?, ?, ?, 0)
    `)
    this.insertItem = this.db.prepare(`
      INSERT INTO items (
        snapshot_id, source_type, owner, item_path, item_id, display_name,
        count, dimension, x, y, z, chunk_x, chunk_z, detail_json
        , enchanted, has_container_data, custom_name, damage, owner_record_id
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `)
    this.insertPlayer = this.db.prepare(`INSERT INTO players
      (snapshot_id, source, name, real_name, xuid, uuid, aliases_text, online, dimension, x, y, z, detail_json)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`)
    this.insertContainer = this.db.prepare(`INSERT INTO containers
      (snapshot_id, source, kind, dimension, chunk_x, chunk_z, x, y, z, occupied_slots, item_count, detail_json)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`)
    this.insertDrop = this.db.prepare(`INSERT INTO drops
      (snapshot_id, source, dimension, chunk_x, chunk_z, entity_count, item_count, distinct_item_count, detail_json)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`)
    this.insertEntity = this.db.prepare(`INSERT INTO entities
      (snapshot_id, source, type_name, custom_name, category, dimension, chunk_x, chunk_z, x, y, z, detail_json)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`)
    this.loadIdentityMap = this.db.prepare(
      'SELECT payload FROM player_identity_maps WHERE server_id = ?'
    )
    this.upsertIdentityMap = this.db.prepare(`
      INSERT INTO player_identity_maps (server_id, updated_at, payload)
      VALUES (?, ?, ?)
      ON CONFLICT(server_id) DO UPDATE SET
        updated_at = excluded.updated_at,
        payload = excluded.payload
    `)
  }

  identityMap(serverId) {
    if (this.identityMaps.has(serverId)) return this.identityMaps.get(serverId)
    const row = this.loadIdentityMap.get(serverId)
    let value = { aliases: {}, profiles: {} }
    if (row) {
      try {
        value = { ...value, ...JSON.parse(row.payload) }
      } catch {
        // 损坏的身份缓存不能阻止读取原始玩家存档。
      }
    }
    this.identityMaps.set(serverId, value)
    return value
  }

  normalizePlayers(serverId, players) {
    const identityMap = this.identityMap(serverId)
    let changed = false
    const grouped = new Map()

    for (const source of players ?? []) {
      const player = {
        ...source,
        realName: source.realName ?? '',
        storageIds: uniqueStrings(source.storageIds).sort()
      }
      const aliases = normalizedPlayerAliases(player)
      const knownKeys = uniqueStrings(aliases.map(alias => identityMap.aliases[alias])).sort()
      let identityKey = knownKeys[0] || aliases[0]
      if (!identityKey) {
        // 没有可靠唯一标识时不按名称合并，确保不会漏掉同名玩家。
        identityKey = `unresolved:${grouped.size}:${player.name ?? ''}`
      }

      for (const oldKey of knownKeys.slice(1)) {
        const oldProfile = identityMap.profiles[oldKey]
        if (oldProfile) {
          identityMap.profiles[identityKey] = {
            ...(identityMap.profiles[identityKey] ?? {}),
            ...oldProfile,
            storageIds: uniqueStrings([
              ...(identityMap.profiles[identityKey]?.storageIds ?? []),
              ...(oldProfile.storageIds ?? [])
            ]).sort()
          }
          delete identityMap.profiles[oldKey]
        }
        for (const [alias, key] of Object.entries(identityMap.aliases)) {
          if (key === oldKey) identityMap.aliases[alias] = identityKey
        }
        changed = true
      }

      const previous = identityMap.profiles[identityKey] ?? {}
      const preferIncomingIdentity = Boolean(player.online || player.source === 'runtime')
      const profile = {
        realName: player.realName || previous.realName || '',
        xuid: preferIncomingIdentity
          ? (player.xuid || previous.xuid || '')
          : (previous.xuid || player.xuid || ''),
        uuid: preferIncomingIdentity
          ? (player.uuid || previous.uuid || '')
          : (previous.uuid || player.uuid || ''),
        storageIds: uniqueStrings([...(previous.storageIds ?? []), ...player.storageIds]).sort()
      }
      if (JSON.stringify(previous) !== JSON.stringify(profile)) changed = true
      identityMap.profiles[identityKey] = profile
      for (const alias of normalizedPlayerAliases(profile)) {
        if (identityMap.aliases[alias] !== identityKey) {
          identityMap.aliases[alias] = identityKey
          changed = true
        }
      }

      player.identityKey = identityKey
      player.realName = profile.realName
      player.xuid = profile.xuid
      player.uuid = profile.uuid
      player.storageIds = profile.storageIds
      player.name = displayPlayerName(player)
      grouped.set(identityKey, mergePlayerRecord(grouped.get(identityKey), player))
    }

    if (changed) {
      this.upsertIdentityMap.run(serverId, Date.now(), JSON.stringify(identityMap))
    }
    return [...grouped.values()]
  }

  createSnapshot(serverId, datasets) {
    const players = this.normalizePlayers(serverId, datasets.players ?? [])
    const containers = datasets.containers ?? []
    const drops = datasets.drops ?? []
    const entities = datasets.entities ?? []
    const createdAt = Date.now()

    this.db.exec('BEGIN IMMEDIATE')
    try {
      const inserted = this.insertSnapshot.run(
        serverId,
        createdAt,
        players.length,
        containers.length,
        drops.length,
        entities.length
      )
      const snapshotId = Number(inserted.lastInsertRowid)

      let itemRecordCount = 0
      const addItem = (item, context, itemPath) => {
        if (!item?.id) return
        this.insertItem.run(
          snapshotId,
          context.sourceType,
          context.owner,
          itemPath,
          item.id,
          item.displayName ?? '',
          Number(item.count ?? item.stackCount ?? 0),
          context.dimension ?? '',
          context.position?.x ?? null,
          context.position?.y ?? null,
          context.position?.z ?? null,
          context.chunkX ?? null,
          context.chunkZ ?? null,
          JSON.stringify(item),
          item.enchanted ? 1 : 0,
          item.hasContainerData ? 1 : 0,
          item.customName ?? '',
          Number(item.damage ?? 0),
          context.recordId ?? null
        )
        itemRecordCount += 1
      }

      for (const player of players) {
        const position = player.position ?? {}
        const insertedPlayer = this.insertPlayer.run(
          snapshotId, player.source ?? '', player.name ?? '', player.realName ?? '',
          player.xuid ?? '', player.uuid ?? '', (player.storageIds ?? []).join('\n'),
          player.online ? 1 : 0, player.dimension ?? '',
          position.x ?? null, position.y ?? null, position.z ?? null, JSON.stringify(player)
        )
        const context = {
          sourceType: 'player',
          owner: player.name || player.xuid || player.uuid || 'unknown-player',
          dimension: player.dimension,
          position: player.position,
          recordId: Number(insertedPlayer.lastInsertRowid)
        }
        walkItems(player.inventory, context, addItem, 'inventory')
        walkItems(player.armor, context, addItem, 'armor')
        walkItems(player.offhand ? [player.offhand] : [], context, addItem, 'offhand')
        walkItems(player.enderChest, context, addItem, 'ender_chest')
      }

      for (const container of containers) {
        const position = container.position ?? {}
        const occupiedSlots = Number(container.occupiedSlots ?? container.items?.length ?? 0)
        const itemCount = Number(container.itemCount ?? (container.items ?? []).reduce((sum, item) => sum + Number(item.count ?? 0), 0))
        const insertedContainer = this.insertContainer.run(
          snapshotId, container.source ?? '', container.kind ?? '', container.dimension ?? '',
          Number(container.chunkX ?? 0), Number(container.chunkZ ?? 0),
          Number(position.x ?? 0), Number(position.y ?? 0), Number(position.z ?? 0),
          occupiedSlots, itemCount, JSON.stringify({ ...container, occupiedSlots, itemCount })
        )
        const context = {
          sourceType: 'container',
          owner: `${container.kind}@${position.x ?? '?'},${position.y ?? '?'},${position.z ?? '?'}`,
          dimension: container.dimension,
          position,
          chunkX: container.chunkX,
          chunkZ: container.chunkZ,
          recordId: Number(insertedContainer.lastInsertRowid)
        }
        walkItems(container.items, context, addItem, 'container')
      }

      for (const chunk of drops) {
        const insertedDrop = this.insertDrop.run(
          snapshotId, chunk.source ?? '', chunk.dimension ?? '',
          Number(chunk.chunkX ?? 0), Number(chunk.chunkZ ?? 0),
          Number(chunk.entityCount ?? 0), Number(chunk.itemCount ?? 0),
          Number(chunk.distinctItemCount ?? chunk.items?.length ?? 0), JSON.stringify(chunk)
        )
        for (const item of chunk.items ?? []) {
          const position = firstPosition(item.positions)
          addItem(
            { ...item, id: item.itemId, count: item.stackCount },
            {
              sourceType: 'drop',
              owner: `chunk:${chunk.chunkX},${chunk.chunkZ}`,
              dimension: chunk.dimension,
              position,
              chunkX: chunk.chunkX,
              chunkZ: chunk.chunkZ,
              recordId: Number(insertedDrop.lastInsertRowid)
            },
            'drop'
          )
        }
      }

      for (const entity of entities) {
        const position = entity.position ?? {}
        this.insertEntity.run(
          snapshotId, entity.source ?? '', entity.typeName ?? '', entity.customName ?? '',
          entity.category ?? '', entity.dimension ?? '', Number(entity.chunkX ?? 0), Number(entity.chunkZ ?? 0),
          Number(position.x ?? 0), Number(position.y ?? 0), Number(position.z ?? 0), JSON.stringify(entity)
        )
      }

      this.db.prepare('UPDATE snapshots SET item_record_count = ? WHERE id = ?')
        .run(itemRecordCount, snapshotId)
      this.db.exec('COMMIT')
      this.prune(serverId)
      return this.getSnapshot(snapshotId)
    } catch (error) {
      this.db.exec('ROLLBACK')
      throw error
    }
  }

  prune(serverId) {
    const stale = this.db.prepare(`
      SELECT id FROM snapshots
      WHERE server_id = ?
      ORDER BY created_at DESC
      LIMIT -1 OFFSET ?
    `).all(serverId, this.keepSnapshots)
    const remove = this.db.prepare('DELETE FROM snapshots WHERE id = ?')
    this.db.exec('BEGIN IMMEDIATE')
    try {
      for (const row of stale) remove.run(row.id)
      this.db.exec('COMMIT')
    } catch (error) {
      this.db.exec('ROLLBACK')
      throw error
    }
  }

  getSnapshot(snapshotId) {
    return this.db.prepare('SELECT * FROM snapshots WHERE id = ?').get(snapshotId) ?? null
  }

  latest(serverId) {
    return this.db.prepare(`
      SELECT * FROM snapshots WHERE server_id = ? ORDER BY created_at DESC LIMIT 1
    `).get(serverId) ?? null
  }

  listSnapshots(serverId, limit = 20) {
    return this.db.prepare(`
      SELECT * FROM snapshots WHERE server_id = ? ORDER BY created_at DESC LIMIT ?
    `).all(serverId, Math.min(Math.max(Number(limit) || 20, 1), 100))
  }

  listData({ serverId, kind, page = 1, pageSize = 50, view = '', sortField = '', sortOrder = '', ...filters }) {
    if (!['players', 'containers', 'drops', 'entities'].includes(kind)) {
      throw new Error(`unknown dataset kind: ${kind}`)
    }
    const snapshot = this.latest(serverId)
    const size = Math.min(Math.max(Number(pageSize) || 50, 1), 500)
    const currentPage = Math.max(Number(page) || 1, 1)
    if (!snapshot) return { items: [], total: 0, page: currentPage, pageSize: size, snapshot: null }

    const table = kind
    const clauses = ['snapshot_id = ?']
    const params = [snapshot.id]
    const addText = (column, value) => {
      const text = String(value ?? '').trim()
      if (!text) return
      clauses.push(`${column} = ?`)
      params.push(text)
    }
    const addNumber = (column, value) => {
      if (value === '' || value == null || !Number.isFinite(Number(value))) return
      clauses.push(`${column} = ?`)
      params.push(Number(value))
    }
    addText('dimension', filters.dimension)
    addText('source', filters.source)
    addNumber('chunk_x', filters.chunkX)
    addNumber('chunk_z', filters.chunkZ)
    addNumber('x', filters.x)
    addNumber('y', filters.y)
    addNumber('z', filters.z)
    if (kind === 'players') {
      const keyword = String(filters.keyword ?? '').trim()
      if (keyword) {
        clauses.push('(name LIKE ? COLLATE NOCASE OR real_name LIKE ? COLLATE NOCASE OR xuid LIKE ? COLLATE NOCASE OR uuid LIKE ? COLLATE NOCASE OR aliases_text LIKE ? COLLATE NOCASE)')
        params.push(...Array(5).fill(`%${keyword}%`))
      }
      if (filters.online !== '' && filters.online != null) {
        clauses.push('online = ?')
        params.push(filters.online === true || filters.online === 'true' ? 1 : 0)
      }
    } else if (kind === 'containers') {
      addText('kind', filters.typeId)
      const name = String(filters.name ?? '').trim()
      if (name) {
        clauses.push(`EXISTS (SELECT 1 FROM items i WHERE i.snapshot_id = containers.snapshot_id
          AND i.source_type = 'container' AND i.owner_record_id = containers.id
          AND (i.display_name LIKE ? COLLATE NOCASE OR i.custom_name LIKE ? COLLATE NOCASE))`)
        params.push(`%${name}%`, `%${name}%`)
      }
    } else if (kind === 'drops') {
      const typeId = String(filters.typeId ?? '').trim()
      const name = String(filters.name ?? '').trim()
      if (typeId || name) {
        const itemClauses = [
          'i.snapshot_id = drops.snapshot_id',
          "i.source_type = 'drop'",
          'i.owner_record_id = drops.id'
        ]
        if (typeId) {
          itemClauses.push('i.item_id = ?')
          params.push(typeId)
        }
        if (name) {
          itemClauses.push('(i.display_name LIKE ? COLLATE NOCASE OR i.custom_name LIKE ? COLLATE NOCASE)')
          params.push(`%${name}%`, `%${name}%`)
        }
        clauses.push(`EXISTS (SELECT 1 FROM items i WHERE ${itemClauses.join(' AND ')})`)
      }
    } else if (kind === 'entities') {
      addText('type_name', filters.typeId)
      addText('category', filters.category)
      const name = String(filters.name ?? '').trim()
      if (name) {
        clauses.push('custom_name LIKE ? COLLATE NOCASE')
        params.push(`%${name}%`)
      }
    }

    const where = clauses.join('\n AND ')
    const direction = sortOrder === 'ascending' ? 'ASC' : 'DESC'
    if (kind === 'entities' && view === 'chunks') {
      const sortColumns = {
        source: 'source', dimension: 'dimension', chunkX: 'chunkX', chunkZ: 'chunkZ',
        entityCount: 'entityCount', naturalCount: 'naturalCount', specialCount: 'specialCount',
        distinctTypeCount: 'distinctTypeCount'
      }
      const order = sortColumns[sortField] ?? 'entityCount'
      const grouped = `SELECT
          CASE WHEN MAX(source = 'runtime') THEN 'runtime' ELSE 'storage' END AS source,
          dimension, chunk_x AS chunkX, chunk_z AS chunkZ,
          COUNT(*) AS entityCount,
          SUM(category = 'NATURAL_MOB') AS naturalCount,
          SUM(category != 'NATURAL_MOB') AS specialCount,
          COUNT(DISTINCT type_name) AS distinctTypeCount
        FROM entities WHERE ${where} GROUP BY dimension, chunk_x, chunk_z`
      const total = this.db.prepare(`SELECT COUNT(*) AS count FROM (${grouped})`).get(...params).count
      const items = this.db.prepare(`${grouped} ORDER BY ${order} ${direction}, dimension, chunkX, chunkZ LIMIT ? OFFSET ?`)
        .all(...params, size, (currentPage - 1) * size)
      return { items, total, page: currentPage, pageSize: size, snapshot }
    }

    const configs = {
      players: {
        select: `id, source, name, real_name AS realName, xuid, uuid, online,
          dimension, x, y, z`,
        sorts: { name: 'name', xuid: 'xuid', uuid: 'uuid', online: 'online', source: 'source', dimension: 'dimension', 'position.x': 'x', 'position.y': 'y', 'position.z': 'z' },
        fallback: 'online DESC, name ASC'
      },
      containers: {
        select: `id, source, kind, dimension, chunk_x AS chunkX, chunk_z AS chunkZ,
          x, y, z, occupied_slots AS occupiedSlots, item_count AS itemCount`,
        sorts: { kind: 'kind', source: 'source', dimension: 'dimension', chunkX: 'chunk_x', chunkZ: 'chunk_z', 'position.x': 'x', 'position.y': 'y', 'position.z': 'z', occupiedSlots: 'occupied_slots', itemCount: 'item_count' },
        fallback: 'dimension ASC, chunk_x ASC, chunk_z ASC'
      },
      drops: {
        select: `id, source, dimension, chunk_x AS chunkX, chunk_z AS chunkZ,
          entity_count AS entityCount, item_count AS itemCount, distinct_item_count AS distinctItemCount`,
        sorts: { source: 'source', dimension: 'dimension', chunkX: 'chunk_x', chunkZ: 'chunk_z', entityCount: 'entity_count', itemCount: 'item_count', distinctItemCount: 'distinct_item_count' },
        fallback: 'item_count DESC'
      },
      entities: {
        select: `id, source, type_name AS typeName, custom_name AS customName, category,
          dimension, chunk_x AS chunkX, chunk_z AS chunkZ, x, y, z`,
        sorts: { typeName: 'type_name', customName: 'custom_name', category: 'category', source: 'source', dimension: 'dimension', chunkX: 'chunk_x', chunkZ: 'chunk_z', 'position.x': 'x', 'position.y': 'y', 'position.z': 'z' },
        fallback: 'type_name ASC'
      }
    }
    const config = configs[kind]
    const order = config.sorts[sortField] ? `${config.sorts[sortField]} ${direction}` : config.fallback
    const total = this.db.prepare(`SELECT COUNT(*) AS count FROM ${table} WHERE ${where}`).get(...params).count
    const rawItems = this.db.prepare(`SELECT ${config.select} FROM ${table} WHERE ${where} ORDER BY ${order}, id ASC LIMIT ? OFFSET ?`)
      .all(...params, size, (currentPage - 1) * size)
    const items = rawItems.map(row => ({
      ...row,
      online: kind === 'players' ? Boolean(row.online) : row.online,
      position: row.x == null ? undefined : { x: row.x, y: row.y, z: row.z },
      x: undefined, y: undefined, z: undefined
    }))
    return {
      items,
      total,
      page: currentPage,
      pageSize: size,
      snapshot
    }
  }

  detail(serverId, kind, id) {
    if (!['players', 'containers', 'drops', 'entities'].includes(kind)) throw new Error('unknown detail kind')
    const snapshot = this.latest(serverId)
    if (!snapshot) return null
    const row = this.db.prepare(`SELECT detail_json FROM ${kind} WHERE snapshot_id = ? AND id = ?`).get(snapshot.id, Number(id))
    return row ? JSON.parse(row.detail_json) : null
  }

  typeOptions(serverId, kind) {
    const snapshot = this.latest(serverId)
    if (!snapshot) return []
    const queries = {
      containers: "SELECT DISTINCT kind AS value, '' AS displayName FROM containers WHERE snapshot_id = ? ORDER BY kind",
      entities: "SELECT DISTINCT type_name AS value, '' AS displayName FROM entities WHERE snapshot_id = ? ORDER BY type_name",
      drops: "SELECT item_id AS value, MAX(display_name) AS displayName FROM items WHERE snapshot_id = ? AND source_type = 'drop' GROUP BY item_id ORDER BY item_id",
      items: 'SELECT item_id AS value, MAX(display_name) AS displayName FROM items WHERE snapshot_id = ? GROUP BY item_id ORDER BY item_id'
    }
    if (!queries[kind]) return []
    return this.db.prepare(queries[kind]).all(snapshot.id).map(row => ({ ...row }))
  }

  entityChunkDetail({ serverId, dimension, chunkX, chunkZ, typeId = '', name = '', category = '', source = '' }) {
    const snapshot = this.latest(serverId)
    if (!snapshot) return null
    const clauses = ['snapshot_id = ?', 'dimension = ?', 'chunk_x = ?', 'chunk_z = ?']
    const params = [snapshot.id, dimension, Number(chunkX), Number(chunkZ)]
    const add = (column, value, like = false) => {
      const text = String(value ?? '').trim()
      if (!text) return
      clauses.push(`${column} ${like ? 'LIKE ? COLLATE NOCASE' : '= ?'}`)
      params.push(like ? `%${text}%` : text)
    }
    add('type_name', typeId)
    add('custom_name', name, true)
    add('category', category)
    add('source', source)
    const rows = this.db.prepare(`SELECT type_name AS typeName, custom_name AS customName, category, x, y, z
      FROM entities WHERE ${clauses.join(' AND ')} ORDER BY type_name, id`).all(...params)
    const types = new Map()
    for (const row of rows) {
      const key = `${row.typeName}\0${row.customName}\0${row.category}`
      const entry = types.get(key) ?? { typeName: row.typeName, customName: row.customName, category: row.category, count: 0, positions: [] }
      entry.count += 1
      entry.positions.push({ x: row.x, y: row.y, z: row.z })
      types.set(key, entry)
    }
    return { types: [...types.values()] }
  }

  searchItems({
    serverId, typeId = '', name = '', sourceType = '', dimension = '', owner = '', itemPath = '',
    enchanted = '', hasContainerData = '', nestedOnly = false,
    countMin = '', countMax = '', xMin = '', xMax = '', yMin = '', yMax = '', zMin = '', zMax = '',
    chunkXMin = '', chunkXMax = '', chunkZMin = '', chunkZMax = '',
    sortField = 'count', sortOrder = 'descending', page = 1, pageSize = 50
  }) {
    const snapshot = this.latest(serverId)
    if (!snapshot) {
      return { items: [], total: 0, page: 1, pageSize, snapshot: null }
    }

    const size = Math.min(Math.max(Number(pageSize) || 50, 1), 200)
    const currentPage = Math.max(Number(page) || 1, 1)
    const clauses = ['snapshot_id = ?']
    const params = [snapshot.id]
    const addText = (column, value) => {
      const text = String(value ?? '').trim()
      if (!text) return
      clauses.push(`${column} = ?`)
      params.push(text)
    }
    const addLike = (column, value) => {
      const text = String(value ?? '').trim()
      if (!text) return
      clauses.push(`${column} LIKE ? COLLATE NOCASE`)
      params.push(`%${text}%`)
    }
    const addRange = (column, operator, value) => {
      if (value === '' || value == null || !Number.isFinite(Number(value))) return
      clauses.push(`${column} ${operator} ?`)
      params.push(Number(value))
    }
    addText('source_type', sourceType)
    addText('item_id', typeId)
    const nameText = String(name ?? '').trim()
    if (nameText) {
      clauses.push('(display_name LIKE ? COLLATE NOCASE OR custom_name LIKE ? COLLATE NOCASE)')
      params.push(`%${nameText}%`, `%${nameText}%`)
    }
    addText('dimension', dimension)
    addLike('owner', owner)
    addLike('item_path', itemPath)
    if (enchanted !== '') addText('enchanted', enchanted === true || enchanted === 'true' ? 1 : 0)
    if (hasContainerData !== '') addText('has_container_data', hasContainerData === true || hasContainerData === 'true' ? 1 : 0)
    if (nestedOnly === true || nestedOnly === 'true') clauses.push("item_path LIKE '%:%'")
    addRange('count', '>=', countMin)
    addRange('count', '<=', countMax)
    addRange('x', '>=', xMin)
    addRange('x', '<=', xMax)
    addRange('y', '>=', yMin)
    addRange('y', '<=', yMax)
    addRange('z', '>=', zMin)
    addRange('z', '<=', zMax)
    addRange('chunk_x', '>=', chunkXMin)
    addRange('chunk_x', '<=', chunkXMax)
    addRange('chunk_z', '>=', chunkZMin)
    addRange('chunk_z', '<=', chunkZMax)

    const searchSortFields = new Set([
      'item_id', 'display_name', 'count', 'source_type', 'owner', 'item_path', 'dimension',
      'x', 'y', 'z', 'chunk_x', 'chunk_z', 'enchanted', 'has_container_data', 'custom_name', 'damage'
    ])
    const orderField = searchSortFields.has(sortField) ? sortField : 'count'
    const orderDirection = sortOrder === 'ascending' ? 'ASC' : 'DESC'
    const where = clauses.join('\n        AND ')

    const total = this.db.prepare(`
      SELECT COUNT(*) AS count FROM items
      WHERE ${where}
    `).get(...params).count

    const items = this.db.prepare(`
      SELECT source_type, owner, item_path, item_id, display_name, count,
             dimension, x, y, z, chunk_x, chunk_z, enchanted,
             has_container_data, custom_name, damage, detail_json
      FROM items
      WHERE ${where}
      ORDER BY ${orderField} ${orderDirection}, item_id ASC
      LIMIT ? OFFSET ?
    `).all(...params, size, (currentPage - 1) * size).map(row => ({
      ...row,
      detail: JSON.parse(row.detail_json),
      detail_json: undefined
    }))

    return { items, total, page: currentPage, pageSize: size, snapshot }
  }

  diff(fromId, toId) {
    const aggregate = snapshotId => this.db.prepare(`
      SELECT item_id, MAX(display_name) AS display_name, SUM(count) AS count
      FROM items WHERE snapshot_id = ? GROUP BY item_id
    `).all(snapshotId)

    const fromSnapshot = this.getSnapshot(fromId)
    const toSnapshot = this.getSnapshot(toId)
    if (!fromSnapshot || !toSnapshot || fromSnapshot.server_id !== toSnapshot.server_id) {
      throw new Error('snapshots do not exist or belong to different servers')
    }

    const rows = new Map()
    for (const item of aggregate(fromId)) {
      rows.set(item.item_id, { itemId: item.item_id, displayName: item.display_name, before: item.count, after: 0 })
    }
    for (const item of aggregate(toId)) {
      const row = rows.get(item.item_id) ?? {
        itemId: item.item_id,
        displayName: item.display_name,
        before: 0,
        after: 0
      }
      row.after = item.count
      rows.set(item.item_id, row)
    }

    const items = [...rows.values()]
      .map(item => ({ ...item, delta: item.after - item.before }))
      .filter(item => item.delta !== 0)
      .sort((left, right) => Math.abs(right.delta) - Math.abs(left.delta))
    return { from: fromSnapshot, to: toSnapshot, items }
  }

  close() {
    this.db.close()
  }
}
