/**
 * @dsh-external/dsh-civhash — CivNexus6 hash 工具包（基于 civhash.exe CLI）。
 * CLI 源码：../ConsoleApplication1/ConsoleApplication1/civhash/civhash.cpp
 * （提取自原工程 ConsoleApplication1.cpp 的 ComputeHash / calculate_bone_hierarchy_hash，
 *   reverse 为已知前缀下的可控字符集暴力反推）
 *
 * 规范：资源注册必须挂 ctx.effect（热重载/卸载自动清理）。
 */
import type { Context } from 'cordis'
import { defineTool } from '@deepseek-ai/dsh-tools'
import { spawn } from 'node:child_process'
import { fileURLToPath } from 'node:url'
import path from 'node:path'
import z from 'schemastery'

export const name = "@dsh-external/dsh-civhash"
export const inject = ['tools']

// 默认 exe 位于插件包 bin/civhash.exe（lib/index.js 的上级 bin 目录）
const DEFAULT_EXE = path.join(path.dirname(path.dirname(fileURLToPath(import.meta.url))), 'bin', 'civhash.exe')

export interface Config {
  /** civhash.exe 路径覆盖（默认用插件自带 bin/civhash.exe） */
  exePath?: string
}

export const Config = z.object({
  exePath: z.string().description('civhash.exe 绝对路径（留空用内置 bin/civhash.exe）').max(500),
})

interface CivArgs {
  text?: string
  bone?: string
  reverse?: string | number
  prefix?: string
  max?: number
  charset?: string
  int?: boolean
  uint?: boolean
  fnv?: boolean
  seed?: string
  fold?: boolean
}

function runExe(exe: string, args: string[], timeoutMs = 120_000): Promise<{ code: number, stdout: string, stderr: string }> {
  return new Promise((resolve, reject) => {
    const child = spawn(exe, args, { windowsHide: true })
    let out = '', err = ''
    let done = false
    const timer = setTimeout(() => {
      if (!done) { child.kill(); reject(new Error('civhash.exe 超时（120s），尝试减小 --max 或缩小 charset')) }
    }, timeoutMs)
    child.stdout.on('data', (d: Buffer) => { out += d.toString('utf8') })
    child.stderr.on('data', (d: Buffer) => { err += d.toString('utf8') })
    child.on('error', (e) => { if (!done) { done = true; clearTimeout(timer); reject(new Error(`无法启动 civhash.exe: ${e.message}`)) } })
    child.on('close', (code) => {
      if (!done) { done = true; clearTimeout(timer); resolve({ code: code ?? -1, stdout: out.trim(), stderr: err.trim() }) }
    })
  })
}

function formatResult(j: any): string {
  if (!j || typeof j !== 'object') return String(j)
  if (j.mode === 'hash') {
    if (j.algo === 'fnv') {
      return [
        `${j.input}`,
        `fnv  = ${j.raw}  (${j.rawValue})`,
        `algo=FNV-1a seed=${j.seed} fold=${j.fold} fmt=${j.fmt}`,
      ].join('\n')
    }
    return [
      `${j.input}`,
      `raw  = ${j.raw}  (${j.rawValue})`,
      `std  = ${j.std}  (${j.stdValue})`,
      `fmt=${j.fmt}；raw=ComputeHash 原语义(无最终异或)，std=标准 CRC32`,
    ].join('\n')
  }
  if (j.mode === 'bone') {
    return [
      `bone : ${j.bone}（叶->根 ${Array.isArray(j.order) ? j.order.join(' -> ') : ''}）`,
      `track: ${j.track}`,
      `hash = ${j.hash}  (${j.hashValue})`,
      `algo = ${(j.algo ?? 'crc').toUpperCase()}${j.raw ? `；raw = ${j.raw}  (${j.rawValue})` : ''}`,
    ].join('\n')
  }
  if (j.mode === 'reverse') {
    const head = `algo=${(j.algo ?? 'crc').toUpperCase()} prefix="${j.prefix}" charsetLen=${j.charsetLen} maxLen=${j.maxLen} 搜索空间 ~${j.space}${j.truncated ? '（已截断，仅显示前 128 个）' : ''}`
    if (Array.isArray(j.results)) {
      const lines = [head]
      for (const r of j.results) {
        lines.push(`target ${r.target} → 命中 ${r.count}`)
        for (const f of r.found as string[]) lines.push(`  ${f === '' ? '(空后缀)' : f}`)
      }
      if (!j.results.some((r: any) => (r.found as string[]).length)) lines.push('所有目标均未命中，可尝试增大 --max、扩大 --charset 或确认前缀/目标值')
      return lines.join('\n')
    }
    const found: string[] = Array.isArray(j.found) ? j.found : []
    const lines = [
      head,
      `target ${j.target} → 命中 ${j.count}`,
    ]
    for (const f of found) lines.push(`  ${f === '' ? '(空后缀)' : f}`)
    if (!found.length) lines.push('未找到候选，可尝试增大 --max、扩大 --charset 或确认前缀/目标值')
    return lines.join('\n')
  }
  return JSON.stringify(j)
}

export function apply(ctx: Context, config: Config): void {
  const cfg: Config = config ?? {}

  ctx.effect(() => ctx.tools.register(defineTool({
    name: '_dsh_external_dsh_civhash_hash',
    description: 'CivNexus6 hash：Civ 风格 CRC32 / 引擎 FNV-1a（--fnv）/ 骨骼链哈希 / CRC+FNV 反推',
    parameters: {
      text: { type: 'string', description: '要哈希的文本（如 TERRAIN_COAST）；reverse 模式可省' },
      bone: { type: 'string', description: '骨骼链路径 Root/A/B（自根到叶，末位为目标骨骼）；提供时 text 作为轨道名' },
      reverse: { type: 'string', description: '目标 CRC32 值（0x 十六进制或十进制，raw 语义）；提供时进入反推模式' },
      prefix: { type: 'string', description: '反推模式：已知字段前缀（默认空）' },
      max: { type: 'number', description: '反推模式：后缀最大长度 1-6（默认 5，越大越慢）' },
      charset: { type: 'string', description: '反推模式：搜索字符集（默认 A-Z 0-9 _）' },
      int: { type: 'boolean', description: '输出有符号 int32（默认 hex；reverse 下无效）' },
      uint: { type: 'boolean', description: '输出无符号 uint32（默认 hex；reverse 下无效）' },
      fnv: { type: 'boolean', description: '使用引擎 FNV-1a（默认 CRC）；开启后 reverse/bone 也切换为 FNV' },
      seed: { type: 'string', description: 'FNV 种子（默认 0x811C9DC5，仅 fnv 生效）' },
      fold: { type: 'boolean', description: 'FNV 大小写折叠变体（A-Z→小写后再异或，仅 fnv）' },
    },
    output: {
      schema: { type: 'string' },
      render: (_args: unknown, value: unknown) => [{ type: 'text', text: String(value) }],
    },
    async execute(args: CivArgs) {
      const a = args ?? {}
      const list: string[] = ['--json']
      if (a.int && a.uint) throw new Error('--int 与 --uint 只能二选一')
      if (a.fnv) {
        list.push('--fnv')
        const sd = a.seed?.trim()
        if (sd) list.push('--seed', sd)
        if (a.fold) list.push('--fold')
      }

      let hint = ''
      const hasReverse = a.reverse !== undefined && a.reverse !== null && String(a.reverse) !== ''
      if (hasReverse) {
        list.push('--reverse', String(a.reverse))
        if (a.prefix) list.push('--prefix', String(a.prefix))
        let m = Number.isFinite(a.max as number) ? Math.trunc(a.max as number) : 5
        m = Math.min(6, Math.max(1, m))
        list.push('--max', String(m))
        if (a.charset) list.push('--charset', String(a.charset))
        if (a.int || a.uint) hint = '\n(note: --int/--uint 在 reverse 模式下无效)'
      } else {
        if (a.prefix || a.max !== undefined || a.charset) hint = '\n(note: prefix/max/charset 仅在 reverse 模式生效，本次已忽略)'
        if (a.int) list.push('--int')
        if (a.uint) list.push('--uint')
        if (!a.text) throw new Error('缺少 text（或改用 reverse 参数进入反推模式）')
        if (a.bone) list.push('--', a.text, '--bone', String(a.bone))
        else list.push('--', a.text)
      }

      const exe = cfg.exePath?.trim() ? cfg.exePath.trim() : DEFAULT_EXE
      const r = await runExe(exe, list)
      if (r.code !== 0 && !r.stdout) throw new Error(r.stderr || `civhash.exe 退出码 ${r.code}`)
      try {
        return formatResult(JSON.parse(r.stdout)) + hint
      } catch {
        return (r.stdout || r.stderr) + hint
      }
    },
  })), '@dsh-external/dsh-civhash: civhash tool')
}
