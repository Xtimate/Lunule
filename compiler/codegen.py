from errors import LangError
from nodes import Num, Str, Var, BinOp, Let, Assign, Print, If, While, Call, FuncDef, Return, ExprStmt, Borrow, ListLit, Index, IndexAssign, For, Spawn

RT_FUNCS = {
    "+": "rt_add", "-": "rt_sub", "*": "rt_mul", "/": "rt_div",
    "<": "rt_lt", ">": "rt_gt", "<=": "rt_le", ">=": "rt_ge",
    "==": "rt_eq", "!=": "rt_ne",
}

BUILTINS = {"open": 2, "read": 1, "write": 2, "close": 1, "len": 1, "trim": 1, "tostr": 1, "push": 2, "channel": 0, "share": 1, "send": 2, "recv": 1}

C_ESC = {"\\": "\\\\", '"': '\\"', "\n": "\\n", "\t": "\\t"}

def c_string(s):
    return '"' + "".join(C_ESC.get(ch, ch) for ch in s) + '"';

class CodeGen:
    def __init__(self):
        self.scopes = []
        self.lines = []
        self.indent = 0
        self.counter = 0
        self.funcs = {}
        self.in_function = False

    def emit(self, text):
        self.lines.append("    " * self.indent + text)

    def lookup(self, node):
        for scope in reversed(self.scopes):
            if node.name in scope["names"]:
                return scope["names"][node.name]
        raise LangError(f"undefined variable '{node.name}'", node.line, node.col)

    def declare(self, name):
        cname = f"v_{name}_{self.counter}"
        self.counter += 1
        self.scopes[-1]["names"][name] = cname
        self.scopes[-1]["owned"].append(cname)
        return cname

    def use(self, node):
        return f'rt_use(&{self.lookup(node)}, "{node.name}")'

    def move(self, node):
        return f'rt_move(&{self.lookup(node)}, "{node.name}")'

    def borrow(self, node):
        return f'rt_borrow(&{self.lookup(node)}, "{node.name}")'

    def operand(self, node):
        return self.borrow(node) if isinstance(node, Var) else self.expr(node)

    def arg_code(self, node):
        return self.borrow(node) if isinstance(node, Borrow) else self.value_of(node)

    def value_of(self, node):
        return self.move(node) if isinstance(node, Var) else self.expr(node)

    def expr(self, node):
        match node:
            case Num(value=v):
                return f"rt_int({v})"
            case Str(value=s):
                return f"rt_str({c_string(s)})"
            case Var():
                return self.use(node)
            case BinOp(op=op, left=left, right=right):
                return f"{RT_FUNCS[op]}({self.operand(left)}, {self.operand(right)})"
            case Call(name=name, args=args, line=line, col=col):
                if name in self.funcs:
                    target, n = f"f_{name}", self.funcs[name]
                elif name in BUILTINS:
                    target, n = f"rt_{name}", BUILTINS[name]
                else:
                    raise LangError(f"undefined function '{name}'", line, col)
                if len(args) != n:
                    raise LangError(
                        f"function '{name}' takes {n} argument(s) but {len(args)} given", line, col
                    )
                arg_list = ", ".join(self.arg_code(a) for a in args)
                return f"{target}({arg_list})"
            case ListLit(items=items):
                if not items:
                    return "rt_list_lit(0, NULL)"
                elems = ", ".join(self.value_of(i) for i in items)
                return f"rt_list_lit({len(items)}, (Value[]){{{elems}}})"
            case Index(index=index):
                return f"rt_index({self.borrow(node)}, {self.operand(index)})"
            case _:
                raise AssertionError(f"unknown expression node {node}")

    def body(self, stmts):
        self.scopes.append({"names": {}, "owned": []})
        self.indent += 1
        for s in stmts:
            self.stmt(s)
        scope = self.scopes.pop()
        for cname in reversed(scope["owned"]):
            self.emit(f"rt_drop(&{cname});")
        self.indent -= 1

    def stmt(self, node):
        match node:
            case Let(name=name, value=value):
                code = self.value_of(value)
                cname = self.declare(name)
                self.emit(f"Value {cname} = {code};")
            case Assign(name=name, value=value):
                cname = self.lookup(node)
                code = self.value_of(value)
                self.emit(f"{{ Value t = {code}; rt_drop(&{cname}); {cname} = t; }}")
            case Print(value=value):
                if isinstance(value, Var):
                    self.emit(f"rt_print({self.use(value)});")
                else:
                    self.emit(f"{{ Value t = {self.expr(value)}; rt_print(t); rt_drop(&t); }}")
            case If(cond=cond, then=then, els=els):
                self.emit(f"if (rt_truthy({self.expr(cond)})) {{")
                self.body(then)
                if els is not None:
                    self.emit("} else {")
                    self.body(els)
                self.emit("}")
            case While(cond=cond, body=body):
                self.emit(f"while (rt_truthy({self.expr(cond)})) {{")
                self.body(body)
                self.emit("}")
            case ExprStmt(value=value):
                self.emit(f"{{ Value t = {self.expr(value)}; rt_drop(&t); }}")
            case Return(value=value, line=line, col=col):
                if not self.in_function:
                    raise LangError("'return' outside of a function", line, col)
                code = "rt_int(0)" if value is None else self.value_of(value)
                self.emit("{")
                self.indent += 1
                self.emit(f"Value ret = {code};")
                for scope in reversed(self.scopes):
                    for cname in reversed(scope["owned"]):
                        self.emit(f"rt_drop(&{cname});")
                self.emit("return ret;")
                self.indent -= 1
                self.emit("}")
            case FuncDef(line=line, col=col):
                raise LangError("functions can only be defined at the top level", line, col)
            case IndexAssign(index=index, value=value):
                self.emit(f"rt_list_set({self.borrow(node)}, {self.operand(index)}, {self.value_of(value)});")
            case For(var=var, iter=iter_, body=body):
                uid = self.counter
                self.counter += 1;
                it, idx = f"it_{uid}", f"i_{uid}"
                src = self.borrow(iter_) if isinstance(iter_, Var) else self.expr(iter_)
                self.emit("{")
                self.indent += 1
                self.emit(f"Value {it} = {src};")
                self.scopes.append({"names": {}, "owned": [it]})
                self.emit(f"for (long {idx} = 0; {idx} < rt_list_len(&{it}); {idx}++) {{")
                self.scopes.append({"names": {}, "owned": []})
                self.indent += 1
                cname = self.declare(var)
                self.emit(f"Value {cname} = rt_list_get(&{it}, {idx});")
                for s in body:
                    self.stmt(s)
                inner = self.scopes.pop()
                for c in reversed(inner["owned"]):
                    self.emit(f"rt_drop(&{c});")
                self.indent -= 1
                self.emit("}")
                outer = self.scopes.pop()
                for c in reversed(outer["owned"]):
                    self.emit(f"rt_drop(&{c});")
                self.indent -= 1
                self.emit("}")
            case Spawn(name=name, args=args, line=line, col=col):
                if name not in self.funcs:
                    raise LangError(f"'{name}' is not a function you defined; spawn needs one of your own functions", line, col)
                n = self.funcs[name]
                if len(args) != n:
                    raise LangError(
                        f"function '{name}' take {n} argument(s) but {len(args)} given", line, col
                    )
                for a in args:
                    if isinstance(a, Borrow):
                        raise LangError("can't lend a value to a thread; move it in (use share() for channels)",
                            a.line, a.col)
                arg_list = ", ".join(self.value_of(a) for a in args)
                if args:
                    self.emit(f"rt_spawn(spawn_f_{name}, {len(args)}, (Value[]){{{arg_list}}});")
                else:
                    self.emit(f"rt_spawn(spawn_f_{name}, 0, NULL);")
            case _:
                raise AssertionError(f"unknown statement node {node}")

    def function(self, fn):
        saved_lines, saved_scopes = self.lines, self.scopes
        self.lines = []
        self.scopes = [{"names": {}, "owned": []}]
        self.in_function = True
        params = [self.declare(p) for p in fn.params]
        self.indent = 1
        for s in fn.body:
            self.stmt(s)
        scope = self.scopes.pop()
        for cname in reversed(scope["owned"]):
            self.emit(f"rt_drop(&{cname});")
        self.emit("return rt_int(0);")
        self.indent = 0
        param_list = ", ".join(f"Value {p}" for p in params) or "void"
        sig = f"Value f_{fn.name}({param_list})"
        text = sig + " {\n" + "\n".join(self.lines) + "\n}\n"
        self.lines, self.scopes = saved_lines, saved_scopes
        self.in_function = False
        return sig, text

def generate(program):
    gen = CodeGen()
    funcs = [s for s in program if isinstance(s, FuncDef)]
    rest = [s for s in program if not isinstance(s, FuncDef)]

    for fn in funcs:
        if fn.name in BUILTINS:
            raise LangError(f"'{fn.name}' is a built-in function and can't be redifined", fn.line, fn.col)
        if fn.name in gen.funcs:
            raise LangError(f"function '{fn.name}' is already defined", fn.line, fn.col)
        gen.funcs[fn.name] = len(fn.params)

    protos, defs = [], []
    for fn in funcs:
        sig, text = gen.function(fn)
        protos.append(sig + ";")
        defs.append(text)
        call_args = ", ".join(f"a[{i}]" for i in range(len(fn.params)))
        wsig = f"Value spawn_f_{fn.name}(Value *a)"
        protos.append(wsig + ";")
        defs.append(wsig + " {\n    (void)a;\n    return f_" + fn.name + "(" + call_args + ");\n}\n")

    gen.body(rest)
    gen.lines.append("    rt_join_all();")
    body = "\n".join(gen.lines)

    out = '#include "rt.h"\n\n'
    if protos:
        out += "\n".join(protos) + "\n\n"
    for d in defs:
        out += d + "\n"
    out += "int main(void) {\n" + body + "\n    return 0;\n}\n"
    return out
