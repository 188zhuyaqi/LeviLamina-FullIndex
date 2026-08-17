import { ref } from 'vue'

const dictionaries = {
  'zh-CN': {
    dimension: {
      overworld: '主世界',
      nether: '下界',
      the_end: '末地'
    },
    source: {
      player: '玩家',
      container: '容器',
      drop: '掉落物',
      entity: '实体',
      runtime: '运行态',
      storage: '存储态'
    },
    category: {
      NATURAL_MOB: '普通生物',
      SPECIAL_ENTITY: '特殊实体'
    },
    dataset: {
      players: '玩家',
      containers: '容器',
      drops: '掉落物',
      entities: '实体'
    },
    phase: {
      starting: '正在启动',
      accepted: '任务已受理',
      scanning: '正在扫描',
      streamed: '正在传输',
      committing: '正在写入索引数据库',
      cancelling: '正在取消',
      cancelled: '已取消',
      complete: '已完成',
      failed: '失败'
    },
    slot: {
      inventory: '背包',
      hotbar: '快捷栏',
      'hotbar:selected': '当前手持槽位',
      armor: '盔甲',
      head: '头盔槽',
      chest: '胸甲槽',
      legs: '护腿槽',
      feet: '靴子槽',
      offhand: '副手',
      ender_chest: '末影箱',
      container: '容器',
      drop: '掉落物'
    },
    container: {
      container: '容器',
      furnace: '熔炉',
      chest: '箱子',
      brewing_stand: '酿造台',
      dispenser: '发射器',
      dropper: '投掷器',
      hopper: '漏斗',
      cauldron: '炼药锅',
      ender_chest: '末影箱',
      shulker_box: '潜影盒',
      blast_furnace: '高炉',
      smoker: '烟熏炉',
      campfire: '营火',
      barrel: '木桶',
      chiseled_bookshelf: '雕纹书架',
      brushable_block: '可疑方块',
      decorated_pot: '饰纹陶罐',
      crafter: '合成器',
      shelf: '置物架'
    },
    enchantment: {
      protection: '保护',
      fire_protection: '火焰保护',
      feather_falling: '摔落缓冲',
      blast_protection: '爆炸保护',
      projectile_protection: '弹射物保护',
      thorns: '荆棘',
      respiration: '水下呼吸',
      depth_strider: '深海探索者',
      aqua_affinity: '水下速掘',
      sharpness: '锋利',
      smite: '亡灵杀手',
      bane_of_arthropods: '节肢杀手',
      knockback: '击退',
      fire_aspect: '火焰附加',
      looting: '抢夺',
      efficiency: '效率',
      silk_touch: '精准采集',
      unbreaking: '耐久',
      fortune: '时运',
      power: '力量',
      punch: '冲击',
      flame: '火矢',
      infinity: '无限',
      luck_of_the_sea: '海之眷顾',
      lure: '饵钓',
      frost_walker: '冰霜行者',
      mending: '经验修补',
      binding_curse: '绑定诅咒',
      vanishing_curse: '消失诅咒',
      impaling: '穿刺',
      riptide: '激流',
      loyalty: '忠诚',
      channeling: '引雷',
      multishot: '多重射击',
      piercing: '穿透',
      quick_charge: '快速装填',
      soul_speed: '灵魂疾行',
      swift_sneak: '迅捷潜行',
      wind_burst: '风爆',
      density: '致密',
      breach: '破甲',
      lunge: '突进'
    },
    entity: {
      'minecraft:allay': '悦灵',
      'minecraft:armadillo': '犰狳',
      'minecraft:armor_stand': '盔甲架',
      'minecraft:arrow': '箭',
      'minecraft:axolotl': '美西螈',
      'minecraft:bat': '蝙蝠',
      'minecraft:bee': '蜜蜂',
      'minecraft:blaze': '烈焰人',
      'minecraft:boat': '船',
      'minecraft:bogged': '沼骸',
      'minecraft:breeze': '旋风人',
      'minecraft:camel': '骆驼',
      'minecraft:cat': '猫',
      'minecraft:cave_spider': '洞穴蜘蛛',
      'minecraft:chest_boat': '运输船',
      'minecraft:chest_minecart': '运输矿车',
      'minecraft:chicken': '鸡',
      'minecraft:cod': '鳕鱼',
      'minecraft:cow': '牛',
      'minecraft:creaking': '嘎枝',
      'minecraft:creeper': '苦力怕',
      'minecraft:dolphin': '海豚',
      'minecraft:donkey': '驴',
      'minecraft:dragon_fireball': '末影龙火球',
      'minecraft:drowned': '溺尸',
      'minecraft:elder_guardian': '远古守卫者',
      'minecraft:ender_crystal': '末影水晶',
      'minecraft:ender_pearl': '末影珍珠',
      'minecraft:enderman': '末影人',
      'minecraft:endermite': '末影螨',
      'minecraft:ender_dragon': '末影龙',
      'minecraft:evocation_illager': '唤魔者',
      'minecraft:falling_block': '下落的方块',
      'minecraft:fireball': '火球',
      'minecraft:fireworks_rocket': '烟花火箭',
      'minecraft:fox': '狐狸',
      'minecraft:frog': '青蛙',
      'minecraft:ghast': '恶魂',
      'minecraft:glow_squid': '发光鱿鱼',
      'minecraft:goat': '山羊',
      'minecraft:guardian': '守卫者',
      'minecraft:hoglin': '疣猪兽',
      'minecraft:hopper_minecart': '漏斗矿车',
      'minecraft:horse': '马',
      'minecraft:husk': '尸壳',
      'minecraft:iron_golem': '铁傀儡',
      'minecraft:leash_knot': '拴绳结',
      'minecraft:llama': '羊驼',
      'minecraft:magma_cube': '岩浆怪',
      'minecraft:minecart': '矿车',
      'minecraft:mooshroom': '哞菇',
      'minecraft:mule': '骡',
      'minecraft:nautilus': '鹦鹉螺',
      'minecraft:ocelot': '豹猫',
      'minecraft:painting': '画',
      'minecraft:panda': '熊猫',
      'minecraft:parrot': '鹦鹉',
      'minecraft:phantom': '幻翼',
      'minecraft:pig': '猪',
      'minecraft:piglin': '猪灵',
      'minecraft:piglin_brute': '猪灵蛮兵',
      'minecraft:pillager': '掠夺者',
      'minecraft:polar_bear': '北极熊',
      'minecraft:pufferfish': '河豚',
      'minecraft:rabbit': '兔子',
      'minecraft:ravager': '劫掠兽',
      'minecraft:salmon': '鲑鱼',
      'minecraft:sheep': '绵羊',
      'minecraft:shulker': '潜影贝',
      'minecraft:shulker_bullet': '潜影弹',
      'minecraft:silverfish': '蠹虫',
      'minecraft:skeleton': '骷髅',
      'minecraft:skeleton_horse': '骷髅马',
      'minecraft:slime': '史莱姆',
      'minecraft:small_fireball': '小火球',
      'minecraft:sniffer': '嗅探兽',
      'minecraft:snow_golem': '雪傀儡',
      'minecraft:snowball': '雪球',
      'minecraft:spider': '蜘蛛',
      'minecraft:squid': '鱿鱼',
      'minecraft:stray': '流浪者',
      'minecraft:strider': '炽足兽',
      'minecraft:tadpole': '蝌蚪',
      'minecraft:thrown_trident': '投掷出的三叉戟',
      'minecraft:trader_llama': '行商羊驼',
      'minecraft:tropicalfish': '热带鱼',
      'minecraft:turtle': '海龟',
      'minecraft:vex': '恼鬼',
      'minecraft:villager': '村民',
      'minecraft:villager_v2': '村民',
      'minecraft:vindicator': '卫道士',
      'minecraft:wandering_trader': '流浪商人',
      'minecraft:warden': '监守者',
      'minecraft:wind_charge_projectile': '风弹',
      'minecraft:witch': '女巫',
      'minecraft:wither': '凋灵',
      'minecraft:wither_skeleton': '凋灵骷髅',
      'minecraft:wither_skull': '凋灵之首',
      'minecraft:wither_skull_dangerous': '危险的凋灵之首',
      'minecraft:wolf': '狼',
      'minecraft:xp_orb': '经验球',
      'minecraft:zoglin': '僵尸疣猪兽',
      'minecraft:zombie': '僵尸',
      'minecraft:zombie_horse': '僵尸马',
      'minecraft:zombie_nautilus': '僵尸鹦鹉螺',
      'minecraft:zombie_pigman': '僵尸猪灵',
      'minecraft:zombie_villager': '僵尸村民',
      'minecraft:zombie_villager_v2': '僵尸村民'
    }
  },
  'en-US': {
    dimension: { overworld: 'Overworld', nether: 'Nether', the_end: 'The End' },
    source: { player: 'Player', container: 'Container', drop: 'Drop', entity: 'Entity', runtime: 'Runtime', storage: 'Storage' },
    category: { NATURAL_MOB: 'Natural mob', SPECIAL_ENTITY: 'Special entity' },
    dataset: { players: 'Players', containers: 'Containers', drops: 'Drops', entities: 'Entities' },
    phase: { starting: 'Starting', accepted: 'Accepted', scanning: 'Scanning', streamed: 'Streaming', committing: 'Committing', cancelling: 'Cancelling', cancelled: 'Cancelled', complete: 'Complete', failed: 'Failed' },
    slot: {},
    container: {},
    enchantment: {
      protection: 'Protection', fire_protection: 'Fire Protection', feather_falling: 'Feather Falling',
      blast_protection: 'Blast Protection', projectile_protection: 'Projectile Protection', thorns: 'Thorns',
      respiration: 'Respiration', depth_strider: 'Depth Strider', aqua_affinity: 'Aqua Affinity',
      sharpness: 'Sharpness', smite: 'Smite', bane_of_arthropods: 'Bane of Arthropods', knockback: 'Knockback',
      fire_aspect: 'Fire Aspect', looting: 'Looting', efficiency: 'Efficiency', silk_touch: 'Silk Touch',
      unbreaking: 'Unbreaking', fortune: 'Fortune', power: 'Power', punch: 'Punch', flame: 'Flame', infinity: 'Infinity',
      luck_of_the_sea: 'Luck of the Sea', lure: 'Lure', frost_walker: 'Frost Walker', mending: 'Mending',
      binding_curse: 'Curse of Binding', vanishing_curse: 'Curse of Vanishing', impaling: 'Impaling',
      riptide: 'Riptide', loyalty: 'Loyalty', channeling: 'Channeling', multishot: 'Multishot', piercing: 'Piercing',
      quick_charge: 'Quick Charge', soul_speed: 'Soul Speed', swift_sneak: 'Swift Sneak', wind_burst: 'Wind Burst',
      density: 'Density', breach: 'Breach', lunge: 'Lunge'
    },
    entity: {}
  }
}

const savedLocale = typeof localStorage === 'undefined' ? null : localStorage.getItem('fullindex.locale')
export const locale = ref(dictionaries[savedLocale] ? savedLocale : 'zh-CN')

export function setLocale(value) {
  if (!dictionaries[value]) return
  locale.value = value
  if (typeof localStorage !== 'undefined') localStorage.setItem('fullindex.locale', value)
  if (typeof document !== 'undefined') document.documentElement.lang = value
}

export function translate(group, value, fallback = value) {
  if (value == null || value === '') return fallback ?? ''
  return dictionaries[locale.value]?.[group]?.[value]
    ?? dictionaries['zh-CN']?.[group]?.[value]
    ?? fallback
}

export const dimensionLabel = value => {
  if (String(value).startsWith('dimension:')) {
    return locale.value === 'zh-CN' ? `维度 ${String(value).slice(10)}` : value
  }
  return translate('dimension', value)
}

export const sourceLabel = value => translate(
  'source',
  value,
  locale.value === 'zh-CN' ? `其他来源（${value}）` : value
)
export const categoryLabel = value => translate(
  'category',
  value,
  locale.value === 'zh-CN' ? `其他分类（${value}）` : value
)
export const datasetLabel = value => translate('dataset', value)
export const phaseLabel = (value, fallback = value) => translate('phase', value, fallback)
export const entityLabel = value => translate('entity', value, locale.value === 'zh-CN' ? '未收录名称' : value)
export const enchantmentLabel = value => {
  const normalized = String(value ?? '').replace(/^minecraft:/, '')
  return translate('enchantment', normalized, normalized.replaceAll('_', ' '))
}

export function containerLabel(value) {
  const kind = String(value ?? '')
  if (kind.startsWith('entity:')) {
    const entityId = kind.slice(7)
    const name = entityLabel(entityId)
    return locale.value === 'zh-CN' ? `实体容器 · ${name}` : `Entity container · ${name}`
  }
  const normalized = kind
    .replace(/^minecraft:/, '')
    .replace(/([a-z0-9])([A-Z])/g, '$1_$2')
    .toLowerCase()
  return translate(
    'container',
    normalized,
    locale.value === 'zh-CN' ? `其他容器（${kind}）` : kind
  )
}

export function slotLabel(value) {
  const raw = String(value ?? '')
  const itemMarker = raw.indexOf(':minecraft:')
  if (itemMarker >= 0) {
    const slot = raw.slice(0, itemMarker)
    const itemId = raw.slice(itemMarker + 1)
    return `${translate('slot', slot)}（${itemId}）`
  }
  if (/^-?\d+$/.test(raw)) return locale.value === 'zh-CN' ? `槽位 ${raw}` : `Slot ${raw}`
  return translate('slot', raw, locale.value === 'zh-CN' ? `槽位 ${raw}` : raw)
}

export function itemPathLabel(value) {
  return String(value ?? '').split('/').filter(Boolean).map(slotLabel).join(' / ')
}

export function ownerLabel(value, sourceType = '') {
  const raw = String(value ?? '')
  if (raw.startsWith('chunk:')) {
    return locale.value === 'zh-CN' ? `区块：${raw.slice(6).replace(',', ', ')}` : raw
  }
  const separator = raw.indexOf('@')
  if (sourceType === 'container' && separator > 0) {
    return `${containerLabel(raw.slice(0, separator))}：${raw.slice(separator + 1).replaceAll(',', ', ')}`
  }
  return raw
}

const errorRules = [
  [/^server (.+) is offline$/, '服务器 $1 当前离线'],
  [/^plugin request timed out: (.+)$/, '插件请求超时：$1'],
  [/^index job (.+) is already running$/, '索引任务 $1 已经在运行'],
  [/^index job is not running$/, '当前没有正在运行的索引任务'],
  [/^index job id does not match$/, '索引任务编号不匹配'],
  [/^an index job is already running$/, '已有索引任务正在运行'],
  [/^plugin rejected index job$/, '插件拒绝了索引任务'],
  [/^plugin rejected cancellation$/, '插件拒绝了取消请求'],
  [/^unauthorized$/, '身份验证失败']
]

export function errorLabel(value) {
  const raw = String(value ?? '')
  if (locale.value !== 'zh-CN') return raw
  for (const [pattern, replacement] of errorRules) {
    if (pattern.test(raw)) return raw.replace(pattern, replacement)
  }
  return raw
}

setLocale(locale.value)
