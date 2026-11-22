"""Restores each changed text file's line endings to what it had at a reference commit.

    python tools/fix_eol.py [reference]

Python's text mode on Windows writes CRLF, so a script that edits an LF file with read_text and
write_text converts the whole file. This puts every file changed since the reference (default
HEAD) back to the reference's line endings; a new file gets LF. Binary files are left alone.
"""
import pathlib
import subprocess
import sys

reference = sys.argv[1] if len(sys.argv) > 1 else "HEAD"
root = pathlib.Path(subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True).stdout.strip())


BINARY_SUFFIXES = {".pdf", ".png", ".jpg", ".jpeg", ".gif", ".webp", ".webm", ".mp4", ".mp3", ".wav", ".flac", ".zip",
                   ".ttf", ".otf", ".woff", ".woff2", ".ico", ".bin", ".exe", ".dll", ".reaperthemezip"}


def is_binary_to_git(name):
    # git diff --numstat prints "-" for the line counts of a file it treats as binary.
    out = subprocess.run(["git", "diff", "--numstat", "--no-index", "--", "/dev/null", name], cwd=root, capture_output=True).stdout
    return out.startswith(b"-\t-\t")


def git(*args):
    return subprocess.run(["git", *args], cwd=root, capture_output=True).stdout


changed = git("diff", "--name-only", reference).decode().split()
changed += git("ls-files", "--others", "--exclude-standard").decode().split()

for name in sorted(set(changed)):
    path = root / name
    if not path.is_file():
        continue
    # Binary files are never touched: by extension, by a NUL near the start, or by git's own test.
    # A PDF can have no NUL in its first 8 KB; rewriting its line endings broke the 1.7 manual.
    if path.suffix.lower() in BINARY_SUFFIXES:
        continue
    data = path.read_bytes()
    if b"\0" in data[:8000] or is_binary_to_git(name):
        continue
    base = subprocess.run(["git", "show", f"{reference}:{name}"], cwd=root, capture_output=True)
    crlf = base.returncode == 0 and b"\r\n" in base.stdout
    fixed = data.replace(b"\r\n", b"\n")
    if crlf:
        fixed = fixed.replace(b"\n", b"\r\n")
    if fixed != data:
        path.write_bytes(fixed)
        print("fixed", name, "(CRLF)" if crlf else "(LF)")
