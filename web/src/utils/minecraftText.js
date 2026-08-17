const colors = {
  0: '#000000', 1: '#0000aa', 2: '#00aa00', 3: '#00aaaa', 4: '#aa0000', 5: '#aa00aa',
  6: '#ffaa00', 7: '#aaaaaa', 8: '#555555', 9: '#5555ff', a: '#55ff55', b: '#55ffff',
  c: '#ff5555', d: '#ff55ff', e: '#ffff55', f: '#ffffff', g: '#ddd605', h: '#e3d4d1',
  i: '#cecaca', j: '#443a3b', m: '#971607', n: '#b4684d', p: '#deb12d', q: '#47a036',
  s: '#2cbaa8', t: '#21497b', u: '#9a5cc6', v: '#fc7812'
}

const freshStyle = defaultColor => ({
  color: defaultColor || null,
  bold: false,
  italic: false,
  obfuscated: false
})

export function parseMinecraftText(value, defaultColor = '') {
  const text = String(value ?? '')
  const segments = []
  let style = freshStyle(defaultColor)
  let buffer = ''

  const flush = () => {
    if (!buffer) return
    segments.push({ text: buffer, ...style })
    buffer = ''
  }

  for (let index = 0; index < text.length; index += 1) {
    if (text[index] !== '\u00a7' || index + 1 >= text.length) {
      buffer += text[index]
      continue
    }

    const code = text[index + 1].toLowerCase()
    const color = colors[code]
    if (color) {
      flush()
      style = { ...freshStyle(defaultColor), color }
      index += 1
      continue
    }
    if (code === 'r') {
      flush()
      style = freshStyle(defaultColor)
      index += 1
      continue
    }
    if (code === 'l' || code === 'o' || code === 'k') {
      flush()
      style = {
        ...style,
        bold: style.bold || code === 'l',
        italic: style.italic || code === 'o',
        obfuscated: style.obfuscated || code === 'k'
      }
      index += 1
      continue
    }

    // Bedrock formatting markers should never leak into visible tooltip text.
    index += 1
  }

  flush()
  return segments
}

export function minecraftSegmentStyle(segment) {
  return {
    color: segment.color || undefined,
    fontWeight: segment.bold ? '700' : undefined,
    fontStyle: segment.italic ? 'italic' : undefined
  }
}
