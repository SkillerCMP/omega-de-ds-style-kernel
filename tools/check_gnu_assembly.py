#!/usr/bin/env python3
"""GNU assembly-only gate; NOT a full libgba kernel build or hardware test.

Uses the same language/mode flags as the Windows devkitARM r54 build. Never
substitutes Clang for GNU GCC. The report records missing tools as NOT RUN.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def has_early_unified_syntax(text: str) -> bool:
    """Require UAL before includes/instructions, matching the Windows guard."""
    return text.lstrip('\ufeff').replace('\r\n', '\n').startswith('.syntax unified\n')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='arm-none-eabi-gcc')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build-rts3-gnu')
    args = parser.parse_args()
    out = args.build_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report = {'status': 'NOT RUN', 'full_kernel_build': False,
              'hardware_test': False, 'compiler_requested': args.compiler,
              'objects': []}
    log: list[str] = []

    def finish(code: int, message: str) -> int:
        report['message'] = message
        (out / 'gnu-assembly-results.json').write_text(json.dumps(report, indent=2) + '\n')
        (out / 'gnu-assembly.log').write_text('\n'.join(log) + '\n')
        print(message)
        return code

    source = ROOT / 'source'
    if not has_early_unified_syntax((source / 'gba_rts_patch.s').read_text(encoding='utf-8-sig')):
        report['status'] = 'FAIL'
        return finish(1, 'FAIL: combined runtime must start with .syntax unified')
    compiler = shutil.which(args.compiler)
    if compiler is None:
        return finish(2, 'NOT RUN: GNU ARM GCC is unavailable; no Clang substitution was made.')
    try:
        version = subprocess.run([compiler, '--version'], capture_output=True, text=True, check=True)
        log.append('$ ' + compiler + ' --version\n' + version.stdout + version.stderr)
        report['compiler_version'] = version.stdout.splitlines()[0]
        if 'gcc' not in report['compiler_version'].lower():
            report['status'] = 'FAIL'
            return finish(1, 'FAIL: requested compiler does not identify itself as GNU GCC.')
        sources = sorted(source.glob('*.s'))
        if not sources:
            raise RuntimeError('No assembly sources found')
        for item in sources:
            obj = out / (item.stem + '.o')
            dep = out / (item.stem + '.d')
            obj.unlink(missing_ok=True)
            cmd = [compiler, '-MMD', '-MP', '-MF', str(dep), '-x', 'assembler-with-cpp',
                   '-g', '-mthumb', '-mthumb-interwork', '-I', str(ROOT / 'build/source'),
                   '-I', str(source), '-c', str(item), '-o', str(obj)]
            result = subprocess.run(cmd, capture_output=True, text=True)
            log.append('$ ' + ' '.join(cmd) + '\n' + result.stdout + result.stderr)
            if result.returncode:
                raise RuntimeError(f'{item.name}: GNU assembler exit {result.returncode}')
            if not obj.is_file() or obj.stat().st_size == 0:
                raise RuntimeError(f'{item.name}: compiler did not produce a nonempty object')
            report['objects'].append({'source': item.name, 'bytes': obj.stat().st_size,
                                      'sha256': hashlib.sha256(obj.read_bytes()).hexdigest().upper()})
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        report['status'] = 'FAIL'
        return finish(1, 'FAIL: ' + str(exc))
    report['status'] = 'PASS'
    return finish(0, f'PASS: {len(report["objects"])} GNU assembly objects; full link/hardware not tested.')


if __name__ == '__main__':
    sys.exit(main())
