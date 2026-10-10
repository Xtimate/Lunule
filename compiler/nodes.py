from dataclasses import dataclass

@dataclass
class Num:
    value: int

@dataclass
class Str:
    value: str

@dataclass
class Var:
    name: str
    line: int
    col: int

@dataclass
class BinOp:
    op: str
    left: object
    right: object

@dataclass
class Let:
    name: str
    value: object
    line: int

@dataclass
class Assign:
    name: str
    value: object
    line: int
    col: int

@dataclass
class Print:
    value: object

@dataclass
class If:
    cond: object
    then: list
    els: object

@dataclass
class While:
    cond: object
    body: list

@dataclass
class Call:
    name: str
    args: list
    line: int
    col: int

@dataclass
class FuncDef:
    name: str
    params: list
    body: list
    line: int
    col: int

@dataclass
class Return:
    value: object
    line: int
    col: int

@dataclass
class ExprStmt:
    value: object

@dataclass
class Borrow:
    name: str
    line: int
    col: int

@dataclass
class ListLit:
    items: list
    line: int
    col: int

@dataclass
class Index:
    name: str
    index: object
    line: int
    col: int

@dataclass
class IndexAssign:
    name: str
    index: object
    value: object
    line: int
    col: int

@dataclass
class For:
    var: str
    iter: object
    body: list
    line: int
    col: int
