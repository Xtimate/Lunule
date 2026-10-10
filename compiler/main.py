import sys
import os, shutil, subprocess, tempfile
from pathlib import Path
from errors import LangError
from lexer import lex
from parser import Parser
from codegen import generate

def runtime_dir():
    base = getattr(sys, "_MEIPASS", None)
    if base:
        return Path(base) / "runtime"
    return Path(__file__).resolve().parent.parent / "runtime"

def build_binary(c_code, out):
    cc = next((c for c in (os.environ.get("CC"), "cc", "gcc", "clang") if c and shutil.which(c)), None)
    if cc is None:
        print("error: no C compiler found (install gcc or clang, or set CC", file=sys.stderr)
        sys.exit(1)
    rt = runtime_dir()
    with tempfile.TemporaryDirectory() as tmp:
        c_file = Path(tmp) / "out.c"
        c_file.write_text(c_code)
        cmd = [cc, "-O2", f"-I{rt}", str(c_file), str(rt / "rt.c"), "-lpthread", "-o", out]
        result = subprocess.run(cmd)
        if result.returncode != 0:
            sys.exit(result.returncode)

def main():
    args = sys.argv[1:]
    out = None
    if "-o" in args:
        i = args.index("-o")
        if i + 1 >= len(args):
            print("usage lunule <file.lun> [-o output]", file=sys.stderr)
            sys.exit(2)
        out = args[i + 1]
        del args[i:i + 2]
    if len(args) != 1:
        print("usage lunule <file.lun> [-o output]", file=sys.stderr)
        sys.exit(2)

    path = args[0]
    with open(path) as f:
        src = f.read()

    try:
        tokens = lex(src)
        program = Parser(tokens).parse_program()
        c_code = generate(program)
    except LangError as e:
        print(f"error: {path}:{e.line}:{e.col}: {e.message}", file=sys.stderr)
        lines = src.split("\n")
        if 1 <= e.line <= len(lines):
            print("  " + lines[e.line - 1], file=sys.stderr)
            print("  " + " " * (e.col - 1) + "^", file=sys.stderr)
        sys.exit(1)

    if out is None:
        print(c_code)
        return
    build_binary(c_code, out)

main()
