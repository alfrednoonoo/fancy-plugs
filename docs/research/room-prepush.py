"""Fancy Room: checks before the one push (docs/room.md, Phase 6). Run from the repo root:

    python -I docs/research/room-prepush.py

It can't compile anything; it catches what went wrong in earlier pushes: a missed
registration, a parameter ID that doesn't exist, raw non-ASCII in C++ strings, line-ending
churn, duplicate test names, includes that don't resolve and unbalanced brackets.
"""
import os
import re
import subprocess
import sys

ROOT = os.getcwd()
failures = []


def read(path):
    with open(os.path.join(ROOT, path), "rb") as f:
        return f.read()


def text(path):
    return read(path).decode("utf-8")


def check(ok, what):
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        failures.append(what)


# 1. Registration ------------------------------------------------------------------------------
places = [
    ("CMakeLists.txt", r"FANCY_ALL_PLUGINS[^\n]*\broom\b"),
    (".github/workflows/build.yml", r"options: \[[^\]]*\broom\]"),
    ("tests/CMakeLists.txt", r"FANCY_HAS_ROOM=1[\s\S]*?RoomTests\.cpp"),
    ("tests/HostTests.cpp", r"#include \"RoomProcessor\.h\""),
    ("tests/HostTests.cpp", r"\{ RoomProcessor p; callback \(p\); \}"),
    ("tests/HostRenderTests.cpp", r"checkRenders<RoomProcessor>"),
    ("tests/PresetTests.cpp", r"\{ RoomProcessor p; check \(p\); \}"),
    ("tests/WorkflowTests.cpp", r"\{ RoomProcessor p; callback \(p\); \}"),
    ("tests/WorkflowTests.cpp", r"FANCY_CHECK_FRESH \(RoomProcessor\)"),
    ("tests/PopupTests.cpp", r"checkEveryPopup<RoomProcessor, RoomEditor>"),
    ("tools/screens/Main.cpp", r"renderHostScales<RoomProcessor, RoomEditor>"),
    ("tools/screens/Main.cpp", r"renderPopups<RoomProcessor, RoomEditor>"),
    ("tools/screens/Main.cpp", r"renderPlugin<RoomProcessor, RoomEditor>"),
    ("tools/hosttest/Main.cpp", r"tailRender \(plugin\);"),
    ("plugins/room/CMakeLists.txt", r"SCREEN_MACRO FANCY_HAS_ROOM"),
]

for path, pattern in places:
    check(re.search(pattern, text(path)) is not None, "registered: %s /%s/" % (path, pattern))

# Every file that includes a plugin's header under #if includes Room's under FANCY_HAS_ROOM.
for path in ["tests/HostTests.cpp", "tests/PresetTests.cpp", "tests/WorkflowTests.cpp", "tests/PopupTests.cpp",
             "tools/screens/Main.cpp", "tests/HostRenderTests.cpp"]:
    check(re.search(r"#if FANCY_HAS_ROOM\r?\n #include \"Room", text(path)) is not None, "include guarded: " + path)

# 2. Parameter IDs ------------------------------------------------------------------------------
processor = text("plugins/room/src/RoomProcessor.cpp")
ids = set(re.findall(r'(?:ParameterID \{ |Parameter \()"(\w+)"', processor)) | set(re.findall(r'toggle \("(\w+)"', processor)) \
    | set(re.findall(r'frequency \("(\w+)"', processor))
check(len(ids) == 21, "21 parameters found: %d (%s)" % (len(ids), ", ".join(sorted(ids))))

used = set()
for path in ["plugins/room/src/RoomEditor.cpp", "plugins/room/src/RoomProcessor.cpp", "tests/RoomTests.cpp",
             "tests/HostRenderTests.cpp"]:
    body = text(path)
    if path == "tests/HostRenderTests.cpp":
        body = body[body.find("#if FANCY_HAS_ROOM\nTEST_CASE"):]
        body = body[:body.find("#endif")]
    used |= set(re.findall(r'(?:parameter|read|isOn|setValue \(processor,|getParameter|text|alternate) \(\s*"(\w+)"', body))
    used |= set(re.findall(r'\{ "(\w+)", -?[\d.]+f \}', body))
    used |= set(re.findall(r'setValue \(processor, "(\w+)"', body))
missing = sorted(used - ids)
check(not missing, "every parameter ID used exists" + (": missing " + ", ".join(missing) if missing else ""))

# 3. Raw non-ASCII bytes in C++ ---------------------------------------------------------------------
changed = subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True).stdout.splitlines()
paths = []
for line in changed:
    path = line[3:].split(" -> ")[-1].strip().strip('"')
    if os.path.isdir(path):
        for base, _, files in os.walk(path):
            paths += [os.path.join(base, f).replace("\\", "/") for f in files]
    else:
        paths.append(path)

cpp = [p for p in paths if p.endswith((".cpp", ".h")) and os.path.isfile(p)]
for path in cpp:
    code = re.sub(r"//[^\n]*", "", text(path))
    code = re.sub(r"/\*[\s\S]*?\*/", "", code)
    literals = re.findall(r'"(?:\\.|[^"\\\n])*"', code)
    raw = [s for s in literals if any(ord(c) > 127 for c in s)]
    check(not raw, "ASCII only in string literals: " + path + (" " + str(raw[:3]) if raw else ""))

# 4. Line endings: never mixed, and the same as the file's committed convention ------------------
for path in paths:
    if os.path.isfile(path) and path.endswith((".cpp", ".h", ".txt", ".md", ".yml", ".py")):
        data = read(path)
        crlf = data.count(b"\r\n")
        lf = data.count(b"\n") - crlf
        check(crlf == 0 or lf == 0, "not mixed: %s (%d CRLF, %d LF)" % (path, crlf, lf))
        head = subprocess.run(["git", "show", "HEAD:" + path], capture_output=True).stdout
        if head:
            head_crlf = head.count(b"\r\n") > 0
            check(head_crlf == (crlf > 0), "same endings as HEAD: " + path)

numstat = subprocess.run(["git", "diff", "--numstat", "HEAD"], capture_output=True, text=True).stdout.splitlines()
for line in numstat:
    added, removed, path = line.split("\t")
    if added != "-" and int(removed) > 50 and path.endswith((".cpp", ".h")):
        check(False, "small diff expected, %s removes %s lines" % (path, removed))

# 5. Unique test names --------------------------------------------------------------------------
names = {}
for name in os.listdir("tests"):
    if name.endswith(".cpp"):
        for test in re.findall(r'TEST_CASE \("([^"]+)"', text("tests/" + name)):
            names.setdefault(test, []).append(name)
dupes = {k: v for k, v in names.items() if len(v) > 1 and k.startswith(("Room", "room"))}
check(not dupes, "unique Room test names" + (": " + str(dupes) if dupes else ""))

# 6. Includes resolve ----------------------------------------------------------------------------
search = ["fancy-core/include"] + ["plugins/%s/src" % p for p in os.listdir("plugins")] + ["tests"]
for path in cpp:
    for inc in re.findall(r'#include ["<]([^">]+)[">]', text(path)):
        if inc.startswith(("juce_", "catch2/")) or "/" not in inc and not inc.endswith(".h"):
            continue
        if inc.endswith("GainTable.h") or inc == "FancyFontData.h":
            continue
        found = any(os.path.isfile(os.path.join(d, inc)) for d in search + [os.path.dirname(path)])
        if not found and not inc.startswith(("fancy/",)) and "." not in inc:
            continue
        check(found or inc in ("algorithm",), "include resolves: %s in %s" % (inc, path))

# 7. Balanced brackets ---------------------------------------------------------------------------
def strip(code):
    code = re.sub(r"//[^\n]*", "", code)
    code = re.sub(r"/\*[\s\S]*?\*/", "", code)
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    code = re.sub(r"'(?:\\.|[^'\\])'", "''", code)
    return code


for path in cpp:
    code = strip(text(path))
    for a, b in ("{}", "()", "[]"):
        check(code.count(a) == code.count(b), "balanced %s%s in %s (%d, %d)" % (a, b, path, code.count(a), code.count(b)))

# 8. Text widths ---------------------------------------------------------------------------------
result = subprocess.run([sys.executable, "-I", "docs/research/room-textfit.py"], capture_output=True, text=True)
check(result.returncode == 0, "text widths: " + result.stdout.strip().splitlines()[-1])

print()
print("%d failures" % len(failures))
sys.exit(1 if failures else 0)
