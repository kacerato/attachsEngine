"""Sample whole-editor Android memory around an already configured bake.

Coordinates are supplied from inspected screenshots. This is an acceptance
driver, not a runtime benchmark: samples do not prove allocator peak or thermal
behavior. No user project is modified by the driver itself.
"""
import argparse
import json
import pathlib
import re
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument("--adb", required=True)
p.add_argument("--transport", default="1")
p.add_argument("--output", type=pathlib.Path, required=True)
p.add_argument("--start", nargs=2, type=int, required=True)
p.add_argument("--cancel", nargs=2, type=int)
p.add_argument("--cancel-delay", type=float, default=.15)
p.add_argument("--seconds", type=float, default=12)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

def adb(*args):
    return subprocess.run([a.adb, "-t", a.transport, *args], check=True,
                          capture_output=True).stdout

def capture(name):
    remote = "/sdcard/physical-budget-sample.png"
    adb("shell", "screencap", "-p", remote)
    adb("pull", remote, str(a.output / name))

samples = []
def sample():
    raw = adb("shell", "dumpsys", "meminfo", "dev.aether.editor").decode()
    (a.output / f"meminfo-{len(samples):02}.txt").write_text(raw, encoding="utf-8")
    def value(label):
        m = re.search(label + r":\s*(\d+)", raw)
        return int(m.group(1)) if m else None
    samples.append({"seconds": time.monotonic() - begin,
                    "totalPssKiB": value("TOTAL PSS"),
                    "totalRssKiB": value("TOTAL RSS")})

begin = time.monotonic()
sample()
adb("shell", "input", "tap", *map(str, a.start))
start_done = time.monotonic() - begin
cancel_done = None
if a.cancel:
    time.sleep(a.cancel_delay)
    adb("shell", "input", "tap", *map(str, a.cancel))
    cancel_done = time.monotonic() - begin
    capture("cancel-requested.png")
else:
    capture("running.png")
sample()
while time.monotonic() - begin < a.seconds:
    sample()
    time.sleep(.5)
capture("after.png")
report = {"scope": "whole editor; sampled PSS, not allocator peak",
          "startInputCompletedSeconds": start_done,
          "cancelInputCompletedSeconds": cancel_done,
          "durationSeconds": time.monotonic() - begin,
          "thermalValidated": False, "samples": samples,
          "maximumSampledPssKiB": max(s["totalPssKiB"] or 0 for s in samples)}
(a.output / "memory.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report))
