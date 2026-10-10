import re
from dataclasses import dataclass
from errors import LangError

@dataclass
class Token:
    kind: str
    text: str
    line: int
    col: int

KEYWORDS = {"let", "print", "if", "else", "while", "fn", "return", "for", "in"}

TOKEN_SPEC = [
    ("COMMENT",  r"#[^\n]*"),
    ("STR",      r'"(?:[^"\\\n]|\\.)*"'),
    ("NUM",      r"\d+"),
    ("ID",       r"[A-Za-z_]\w*"),
    ("OP",       r"==|!=|<=|>=|[+\-*/=()<>{},&\[\]]"),
    ("NEWLINE",  r"\n"),
    ("SKIP",     r"[ \t\r]+"),
    ("MISMATCH", r"."),
]
TOKEN_RE = re.compile("|".join(f"(?P<{name}>{pat})" for name, pat in TOKEN_SPEC))

def lex(src):
    tokens = []
    line, line_start = 1, 0
    for m in TOKEN_RE.finditer(src):
        kind, text = m.lastgroup, m.group()
        col = m.start() - line_start + 1

        match kind:
            case "NEWLINE":
                tokens.append(Token("NEWLINE", text, line, col))
                line += 1
                line_start = m.end()
            case "SKIP" | "COMMENT":
                continue
            case "MISMATCH":
                raise LangError(f"unexpected character {text!r}", line, col)
            case "ID" if text in KEYWORDS:
                tokens.append(Token(text, text, line, col))
            case "OP":
                tokens.append(Token(text, text, line, col))
            case _:
                tokens.append(Token(kind, text, line, col))

    tokens.append(Token("EOF", "", line, 1))
    return tokens
