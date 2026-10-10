# LANGNAME

Lunule is a programming language I made from scratch for Hack Club Crescent. It compilesto native code, has no garbage collector, and you never have to write `free`. It also can't have data races, which is the par i'm most proud of.

```
fn worker(ch, n) {
  send(&ch, n * n)
}

let ch = channel()
spawn worker(share(&ch), 7)
print(recv(&ch))
```

## wait why

i wanted to see if you could get the "file close themselves and threads don't run into eachother" thing from Rust ownership, but without types, turns out you can, as long as you're okay with some stuff only getting caught when the code actually runs (more on that below)

the idea is super simple. every value has exactly one owner. when the owner goes out of scope the value is freed. that's it. that's the whole language

## how it works

```
source -> lexer -> parser -> code generator -> C -> cc -> native binary
```

the compiler is written in python (`compiler/`) and spits out a C file. that gets linked with a small C runtime (`runtime/`) that handles all the ownership stuff.

every value carries a type tag and a "borrowed" flag, and the scopes in the generated C drop everything they own when they end. that include early `return`, `break`, and `continue`, which took me an embarrassing amount of time to get right

## quick tour

### the basics

```
let x = 1 + 2 * 3
let name = "world"
print("hello " + name)

let i = 3
while i > 0 {
    print(i)
    i = i - 1
}

if x > 5 {
    print("big")
} else {
    print("small")
}
```

comments start with `#`. strings do `\n`, `\t`, `\"` and `\\`. ints only, no floats (sorry)

### functions

```
fn fact(n) {
    if n < 2 {
      return 1
    }
    return n * fact(n - 1)
}
print(fact(5))
```

you can call a function before you define it. they can'y see variables from the outside though, everything has to come in through the parameters

### ownership (the main thing)

```
let a = "hello"
let b = a         # the string moves to b
print(b)          # hello
print(a)          # runtime error: use of moved value 'a'
```

passing a variable to a function moves it too. ints just get copied because they're boring

### borrowing

```
fn greet(name) {
    print(name)
}

let s = "hi"
greet(&s)       # lend it out
print(s)        # still mine
```

`&x` only works when you pass it straight into a function call. the function can read or lend it on, but it can't keep it or free it. this one rule means i didn't need lifetimes, which i am very happy about

### lists

```
let xs = [1, 2, 3]
push(&xs, 4)
xs[0] = 10
print(xs)             # [10, 2, 3, 4]
print(len(&xs))       # 4

for x in xs {
    print(x)
}
```

lists own their elements. only ints and strings fit in them. when you read a string out of a list you get a copy, so you can't end up pointing at something that got freed

### files

```
let f = open("notes.txt", "w")
write(&f, "hello\n")
close(f)

let g = open("notes.txt", "r")
let text = read(g)
print(trim(&text))
```

if you forget to `close` a file it closes itself when it goes out of scope. this is the whole reason i built the language lol

### threads and channels

```
fn shout(ch, s) {
    send(&ch, s + "!")
}

let ch = channel()
let word = "hey"
spawn shout(share(&ch), word)     # word moves into the thread
print(recv(&ch))                  # hey!
```

- `spawn` runs one of your functions on a new thread. arguments get moved in, and trying to borrow into a thread is a compile error
- `send` moves a value into the channel, `recv` waits until something shows up and hands it to you
- `share(&ch)` gives another handle to the same channel
- main waits for all threads before exiting
- if a `recv` could never finish, it panics instead of hanging forever

you can even send a file handle to another thread. it just only has one owner at a time

## built-ins

| function | what it does |
|---|---|
| `open(path, mode)` | open a file (`"r"`, `"w"`, `"a"`) |
| `read(f)` | read the whole file into a string, then close it |
| `write(&f, s)` | write a string |
| `close(f)` | close a file |
| `len(x)` | length of a string or list |
| `trim(s)` | strip whitespace off both ends |
| `tostr(x)` | int to string |
| `push(&xs, v)` | add to a list |
| `channel()` | make a channel |
| `share(&ch)` | another handle to the same channel |
| `send(&ch, v)` | move a value into a channel |
| `recv(&ch)` | take a value out of a channel (waits) |

## the showcase

`examples/showcase/stats.lun` starts 4 threads. each one reads a file, measures it, writes a little report, and tells main it's done over a channel. main collects everything:

```
build/in_0.txt: raw=16 trimmed=11
build/in_1.txt: raw=5 trimmed=4
build/in_2.txt: raw=28 trimmed=17
build/in_3.txt: raw=1 trimmed=1
total bytes: 50
```

there's not a single `free` or `close` in the whole program. it just works. make a `build/` folder first, because lunule can't make folders yet

## the good and the bad

| good | bad |
|---|---|
| everything gets cleaned up right when it should, no GC pauses | you can't just share stuff around, using something after you moved it is an error |
| no data races, because two threads can never hold the same thing | no shared mutable state between threads, you have to use channels |
| native speed (via C) | every value has a tag and a flag, so it's a bit heavier than it needs to be |
| no types to write | type errors and use-after-move only show up when that line actually runs |
| borrowing without lifetimes | a borrow can only go down into a call, never be stored or returned |

## stuff that doesn't work (yet)

being honest here:

- **errors just crash the program.** missing file, bad index, whatever. there's no try/catch and i haven't figured out what a good version of that looks like
- **ownership is checked while the program runs**, not before. so a use-after-move only gets caught if that line actually executes. a proper static checker would fix this and that's the next thing i want to build
- **no text parsing.** there's no split, no substring, and no way to turn a string into a number
- **lists only hold ints and strings**, no lists of lists
- **strings are C strings**, so no zero bytes in them
- **ints are 64-bit** with no overflow checks, and no floats
- **no closures**, functions are top-level only
- **deadlock detection is basic.** `recv` only panics when nobody else has a handle to the channel
- **you need gcc or clang**, the generated C uses a GNU extension

## running it

you need python 3.10+, gcc or clang, and pthreads.

```
python compiler/main.py program.lun -o program
./program
```

or grab a build from the releases page and run `./lunule program.lun -o program`. you still need gcc or clang installed for that, because lunule compiles to C

compile errors point at the exact spot:

```
error: tests/t4.lun:1:7: undefined variable 'y'
  print(y)
        ^
```

## things that wrent wrong while making this

- the compile was correct the whole time, and a whole evening of debugging a hanging channel test turned out to be `&` instead of `%` in one line of the runtime..... one character
- C doesn't define the order function arguments get evaluated in, so `recv(&ch) - recv(&ch)` was giving me `-7` instead of `7`. that one needed a real fix
- i renamed the language halfway through and nearly shipped with the old name in a few places

made for [Hack Club Crescent](https://crescent.hackclub.com) by Xtimate
