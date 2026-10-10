from errors import LangError
from nodes import Num, Str, Var, BinOp, Let, Assign, Print, If, While, Call, FuncDef, Return, ExprStmt, Borrow, ListLit, Index, IndexAssign, For

CMP_OPS = ("<", ">", "<=", ">=", "==", "!=")
ESCAPES = {"n": "\n", "t": "\t", "\\": "\\", '"': '"'}

def describe(tok):
    if tok.kind == "EOF":     return "end of file"
    if tok.kind == "NEWLINE": return "end of line"
    return repr(tok.text)

def unescape(tok):
    body = tok.text[1:-1]
    out = []
    i = 0
    while i < len(body):
        ch = body[i]
        if ch == "\\":
            nxt = body[i + 1]
            if nxt not in ESCAPES:
                raise LangError(f"unknown escape '\\{nxt}'", tok.line, tok.col)
            out.append(ESCAPES[nxt])
            i += 2
        else:
            out.append(ch)
            i += 1
    return "".join(out)

class Parser:
    def __init__(self, tokens):
        self.toks = tokens
        self.pos = 0

    def peek(self):
        return self.toks[self.pos]

    def advance(self):
        tok = self.toks[self.pos]
        self.pos += 1
        return tok

    def expect(self, kind):
        tok = self.peek()
        if tok.kind != kind:
            what = {"ID": "a name"}.get(kind, repr(kind))
            raise LangError(f"expected {what} but found {describe(tok)}", tok.line, tok.col)
        return self.advance()

    def parse_program(self):
        return self.statements("EOF")

    def statements(self, until):
        stmts = []
        while self.peek().kind not in (until, "EOF"):
            if self.peek().kind == "NEWLINE":
                self.advance()
                continue
            stmts.append(self.statement())
            tok = self.peek()
            if tok.kind not in ("NEWLINE", "EOF", "}"):
                raise LangError(f"unexpected {describe(tok)} after statement", tok.line, tok.col)
        return stmts

    def block(self):
        self.expect("{")
        stmts = self.statements("}")
        self.expect("}")
        return stmts


    def statement(self):
        tok = self.peek()
        match tok.kind:
            case "let":
                self.advance()
                name = self.expect("ID")
                self.expect("=")
                return Let(name.text, self.expr(), name.line)
            case "ID":
                name = self.advance()
                if self.peek().kind == "(":
                    call = Call(name.text, self.args(), name.line, name.col)
                    return ExprStmt(call)
                if self.peek().kind == "[":
                    self.advance()
                    index = self.expr()
                    self.expect("]")
                    self.expect("=")
                    return IndexAssign(name.text, index, self.expr(), name.line, name.col)
                self.expect("=")
                return Assign(name.text, self.expr(), name.line, name.col)
            case "print":
                self.advance()
                self.expect("(")
                value = self.expr()
                self.expect(")")
                return Print(value)
            case "if":
                return self.if_stmt()
            case "while":
                self.advance()
                cond = self.expr()
                return While(cond, self.block())
            case "fn":
                return self.func_def()
            case "return":
                self.advance()
                value = None
                if self.peek().kind not in ("NEWLINE", "EOF", "}"):
                    value = self.expr()
                return Return(value, tok.line, tok.col)
            case "for":
                self.advance()
                var = self.expect("ID")
                self.expect("in")
                iterable = self.expr()
                return For(var.text, iterable, self.block(), tok.line, tok.col)
            case _:
                raise LangError(f"expected a statement but found {describe(tok)}", tok.line, tok.col)

    def arg(self):
        if self.peek().kind == "&":
            amp = self.advance()
            name = self.expect("ID")
            return Borrow(name.text, amp.line, amp.col)
        return self.expr()

    def if_stmt(self):
        self.advance()
        cond = self.expr()
        then = self.block()
        els = None
        if self.peek().kind == "else":
            self.advance()
            if self.peek().kind == "if":
                els = [self.if_stmt()]
            else:
                els = self.block()
        return If(cond, then, els)

    def func_def(self):
        self.advance()
        name = self.expect("ID")
        self.expect("(")
        params = []
        if self.peek().kind != ")":
            params.append(self.expect("ID"))
            while self.peek().kind == ",":
                self.advance()
                params.append(self.expect("ID"))
        self.expect(")")
        seen = set()
        for p in params:
            if p.text in seen:
                raise LangError(f"duplicate parameter '{p.text}'", p.line, p.col)
            seen.add(p.text)
            body = self.block()
            return FuncDef(name.text, [p.text for p in params], body, name.line, name.col)

    def args(self):
        self.expect("(")
        args = []
        if self.peek().kind != ")":
            args.append(self.arg())
            while self.peek().kind == ",":
                self.advance()
                args.append(self.arg())
        self.expect(")")
        return args

    def expr(self):
        node = self.arith()
        if self.peek().kind in CMP_OPS:
            op = self.advance()
            node = BinOp(op.kind, node, self.arith())
        return node

    def arith(self):
        node = self.term()
        while self.peek().kind in ("+", "-"):
            op = self.advance()
            node = BinOp(op.kind, node, self.term())
        return node

    def term(self):
        node = self.atom()
        while self.peek().kind in ("*", "/"):
            op = self.advance()
            node = BinOp(op.kind, node, self.atom())
        return node

    def atom(self):
        tok = self.peek()
        match tok.kind:
            case "NUM":
                self.advance()
                return Num(int(tok.text))
            case "STR":
                self.advance()
                return Str(unescape(tok))
            case "ID":
                self.advance()
                if self.peek().kind == "(":
                    return Call(tok.text, self.args(), tok.line, tok.col)
                if self.peek().kind == "[":
                    self.advance()
                    index = self.expr()
                    self.expect("]")
                    return Index(tok.text, index, tok.line, tok.col)
                return Var(tok.text, tok.line, tok.col)
            case "(":
                self.advance()
                node = self.expr()
                self.expect(")")
                return node
            case "&":
                raise LangError("a borrow '&' can only be passed directly as a function argument",
                                tok.line, tok.col)
            case "[":
                self.advance()
                items = []
                if self.peek().kind != "]":
                    items.append(self.expr())
                    while self.peek().kind == ",":
                        self.advance()
                        items.append(self.expr())
                self.expect("]")
                return ListLit(items, tok.line, tok.col)
            case _:
                raise LangError(f"expected a value but found {describe(tok)}", tok.line, tok.col)
