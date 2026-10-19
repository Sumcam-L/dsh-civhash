#!/usr/bin/env node
/**
 * Smoke tests for the bundled civhash.exe.
 * Verifies the known vectors documented in README against the actual binary,
 * so a rebuilt exe that changes hashing semantics fails loudly.
 *
 *   node scripts/test.mjs
 */
import { spawnSync } from 'node:child_process'
import { fileURLToPath } from 'node:url'
import path from 'node:path'

const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)))
const exe = process.env.CIVHASH_EXE || path.join(root, 'bin', 'civhash.exe')

let failed = 0
let passed = 0

function run(args) {
  const r = spawnSync(exe, args, { encoding: 'utf8' })
  if (r.error) throw r.error
  if (r.status !== 0) throw new Error(`civhash exited ${r.status}: ${r.stderr.trim()}`)
  return r.stdout.trim()
}

function json(args) {
  const out = run([...args, '--json'])
  try {
    return JSON.parse(out)
  } catch {
    throw new Error(`expected JSON, got: ${out}`)
  }
}

function check(name, actual, expected) {
  if (actual === expected) {
    passed++
  } else {
    failed++
    console.error(`FAIL ${name}\n  expected: ${expected}\n  actual  : ${actual}`)
  }
}

// --- CRC32: standard vector + Civ raw semantics ---------------------------
{
  const r = json(['123456789'])
  check('crc32 std vector', r.std, '0xCBF43926')
  check('crc32 raw vector', r.raw, '0x340BC6D9')
}

// raw = std ^ 0xFFFFFFFF, always ------------------------------------------
{
  const r = json(['TERRAIN_COAST'])
  check('raw/std xor relation',
    ((parseInt(r.raw, 16) ^ parseInt(r.std, 16)) >>> 0), 0xFFFFFFFF)
  check('TERRAIN_COAST raw', r.raw, '0x4A707A11')
}

// --- FNV-1a --------------------------------------------------------------
{
  check('fnv default seed/input', json(['abc', '--fnv']).raw, '0x1A47E90B')
  check('fnv fold', json(['abc', '--fnv', '--fold', '--seed', '0x01000193']).raw, '0xBDD5594D')
}

// --- bone hierarchy ------------------------------------------------------
{
  const r = json(['Idle', '--bone', 'Root/Spine/Head'])
  check('bone mode', r.mode, 'bone')
  check('bone leaf->root order', JSON.stringify(r.order), JSON.stringify(['Head', 'Spine', 'Root']))
  check('bone hash', r.hash, '0xFF0361A1')
}

// --- reverse search ------------------------------------------------------
{
  const r = json(['--reverse', '0x16D22538', '--prefix', 'TERRA', '--max', '2'])
  check('crc reverse hit', JSON.stringify(r.found), JSON.stringify(['7']))
}
{
  const r = json(['--reverse', '0x3C250997', '--fnv', '--prefix', 'UNIT_ARCHER', '--max', '1'])
  check('fnv reverse hit', JSON.stringify(r.found), JSON.stringify(['']))
}

// --- output formats ------------------------------------------------------
{
  const r = json(['abc', '--int'])
  check('int format', r.fmt, 'int')
  check('int is signed', r.rawValue < 0, true)
}
{
  const r = json(['abc', '--uint'])
  check('uint format', r.fmt, 'uint')
  check('uint is unsigned', r.rawValue >>> 0, r.rawValue)
}

// --- exit codes ----------------------------------------------------------
{
  const r = spawnSync(exe, ['--help'], { encoding: 'utf8' })
  check('--help exits 0', r.status, 0)
  check('--help prints usage', /civhash/.test(r.stdout), true)
}
{
  const r = spawnSync(exe, ['text', '--name'], { encoding: 'utf8' })
  check('unknown flag exits non-zero', r.status !== 0, true)
}

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
