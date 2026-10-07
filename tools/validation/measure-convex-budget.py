"""Measure the real native bake probe and Windows process peak working set.

This records the complete probe process (including optional project export),
not allocator bytes attributed exclusively to V-HACD and not a thermal test.
"""
import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import platform
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('report', type=Path)
parser.add_argument('--project', type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
args.report.parent.mkdir(parents=True, exist_ok=True)


class MemoryCounters(ctypes.Structure):
    _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            'PeakWorkingSetSize', 'WorkingSetSize', 'QuotaPeakPagedPoolUsage',
            'QuotaPagedPoolUsage', 'QuotaPeakNonPagedPoolUsage',
            'QuotaNonPagedPoolUsage', 'PagefileUsage', 'PeakPagefileUsage')]


memory_info = ctypes.WinDLL('psapi').GetProcessMemoryInfo
memory_info.argtypes = [wintypes.HANDLE, ctypes.POINTER(MemoryCounters), wintypes.DWORD]
memory_info.restype = wintypes.BOOL
command = [str(repo / 'build/editor-host/aether_gui_preview.exe'),
           'measure-convex-budget', str(args.report.resolve())]
if args.project:
    command.append(str(args.project.resolve()))
peak = 0
with args.report.with_suffix('.log').open('w', encoding='utf-8') as log:
    process = subprocess.Popen(command, cwd=repo, stdout=log, stderr=log)
    while True:
        counters = MemoryCounters()
        counters.cb = ctypes.sizeof(counters)
        if memory_info(process._handle, ctypes.byref(counters), counters.cb):
            peak = max(peak, counters.PeakWorkingSetSize)
        if process.poll() is not None:
            break
        time.sleep(.02)
if process.returncode:
    raise SystemExit(f'Native probe failed: {process.returncode}; see {log.name}')
record = json.loads(args.report.read_text(encoding='utf-8'))
record.update(host=platform.platform(), peakProcessWorkingSetBytes=peak,
              memoryScope='entire native probe process',
              projectExportIncluded=bool(args.project), thermalValidated=False)
args.report.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print(json.dumps(record, indent=2))
