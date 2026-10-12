#!/usr/bin/env python3
"""Boot one available iPhone simulator and verify the app launches successfully."""
import json
from pathlib import Path
import subprocess
import sys
import time

app = Path(sys.argv[1]).resolve()
def run(*args):
    return subprocess.check_output(["xcrun", "simctl", *args], text=True)
devices = json.loads(run("list", "devices", "available", "--json"))["devices"]
candidates = [d for runtime, entries in devices.items() if "iOS" in runtime for d in entries if "iPhone" in d["name"]]
if not candidates:
    sys.exit("No iPhone simulator runtime installed")
device = candidates[0]
udid = device["udid"]
if device["state"] != "Booted":
    run("boot", udid)
run("bootstatus", udid, "-b")
run("install", udid, str(app))
print(run("launch", udid, "fr.garwig.linksawakeningdx"))
time.sleep(5)
# terminate fails when the app exited or crashed before this smoke check.
print(run("terminate", udid, "fr.garwig.linksawakeningdx"))
print("PASS: app launched and remained alive in", device["name"])
