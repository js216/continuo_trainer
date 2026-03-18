# Web App Transition

General plan:

1. REST API in all/stats for resource management
2. Centralize I/O
3. Translate into Rust
4. Combine the front-end logic as a single app (no_std?)
5. Convert gui/synth/midi to js/html/css
6. Make a web app!

Source code divides into three groups:

1. Main logic: keep pure (basically `no_std` besides stdin/stdout)

   - all.lua
   - stats.lua
   - group.rs
   - karaoke.rs
   - rules.lua

2. Platform-specific: no change (later add JavaScript versions)

   - synth.c
   - midi.c
   - gui.cpp
   - play.dot
   - run.c

3. Helpers: no change (backend only)

   - entry.rs
   - g2ly.rs
   - l2ly.rs
   - report.lua
   - run2dot.py
   - tst.lua

### Stage 1: REST API conversion for stats/all

all.lua and stats.lua need to be rewritten so that they only to stdin/stdout,
 while any kind of file management and other OS interactions are done via a
backend using REST-like APIs. In other words, all/stats will do only
stdin/stdout while a new program, files.c, will handle the requests for side
effects. This will allow a future rewrite of all/stats into web browser front
end, while files.c will evolve into the server backend. Prepare a plan for
this, in particular, show what new commands all/stats will emit and receive,
and then which commands files.c will emit and receive.

New commands in stats/all and the new files.c:

---

## Plan: Stage 1 implementation — files.c

### Inventory of OS interactions to remove

**all.lua** currently does:
- `io.popen("ls seq/*.txt")` — list lessons
- `io.popen("ls chn/*.txt")` — list chunks (stale check)
- `io.open("seq/<n>.txt", "rb")` — read lesson file
- `io.open("chn/<hash>.txt", "rb/wb")` — read/write chunk files
- `io.popen("sha1sum <tmp>")` + `os.tmpname()` + `os.remove()` — hash content
- `os.execute("mkdir -p chn")` — create directory

**stats.lua** currently does:
- `io.open(path, "r")` + `loadfile(path)` — load stats
- `io.open(path, "w")` + `f:write(...)` — save stats
- `os.time()`, `os.date()`, `os.difftime()` — wall clock / date arithmetic

### Protocol: FILE_* request/response

All content is base64-encoded to stay on one line.
Requests are sent on stdout by all.lua or stats.lua.
Responses arrive on their stdin, mixed with normal protocol traffic.
Each program uses a `file_req(line)` helper that writes the request and
reads stdin in a loop, buffering non-`FILE_` lines for later, until the
matching `FILE_` response line arrives.

```
Request                          Response
────────────────────────────     ────────────────────────────────────────
FILE_LIST_SEQ                →  FILE_LIST_SEQ_RESULT <n1> <n2> ...
FILE_LIST_CHN                →  FILE_LIST_CHN_RESULT <hash1> <hash2> ...
FILE_READ_SEQ <n>            →  FILE_CONTENT <b64>
                                FILE_NOT_FOUND seq/<n>.txt
FILE_READ_CHN <hash>         →  FILE_CONTENT <b64>
                                FILE_NOT_FOUND chn/<hash>.txt
FILE_STORE_CHN <b64>         →  FILE_STORED <hash>      (written or already present)
                                FILE_COLLISION <hash>   (hash match, content differs)
FILE_READ_STATS              →  FILE_CONTENT <b64>
                                FILE_NOT_FOUND
FILE_WRITE_STATS <b64>       →  FILE_WRITE_OK
FILE_NOW                     →  FILE_NOW_RESULT <unix-ts> <YYYY-MM-DD>
FILE_TIMESTAMP <YYYY-MM-DD>  →  FILE_TIMESTAMP_RESULT <unix-ts>
```

`FILE_STORE_CHN` subsumes sha1 + collision-check + write into one round-trip:
files.c computes SHA1 internally, checks `chn/<hash>.txt`, and responds.

`FILE_NOW` satisfies both `os.time()` and `os.date("%Y-%m-%d")` in one call.
`FILE_TIMESTAMP` converts a stored date string back to a Unix timestamp so
stats.lua can compute day-differences with plain subtraction (no os.difftime).

### Changes to all.lua

- Remove: all `io.popen`, `io.open`, `os.execute`, `os.tmpname`, `os.remove`.
- Add `file_req(request_line) → response_line` helper (buffers non-FILE_ input).
- Add base64 encode/decode (small pure-Lua implementation).
- `sha1(content)` + `write_chunk(hash, content)` → single `file_req("FILE_STORE_CHN " .. b64(content))`, returns hash from `FILE_STORED <hash>`.
- `collect_lessons()` → `file_req("FILE_LIST_SEQ")`, parse `FILE_LIST_SEQ_RESULT`.
- Stale-chn check → `file_req("FILE_LIST_CHN")`, parse `FILE_LIST_CHN_RESULT`.
- `io.open("seq/<n>.txt")` → `file_req("FILE_READ_SEQ <n>")`, decode `FILE_CONTENT`.
- `load_chunk(hash)` file read → `file_req("FILE_READ_CHN <hash>")`.

### Changes to stats.lua

- Remove: `io.open`, `loadfile`, `os.time`, `os.date`, `os.difftime`.
- Add same `file_req` helper and base64 codec.
- `load_stats(path)`: request `FILE_READ_STATS`, decode b64 → string, parse with `load(str)()`.
- `save_stats(path, data)`: serialize table to string (existing logic), encode b64, `FILE_WRITE_STATS <b64>`, await `FILE_WRITE_OK`.
- `get_date_str()` → `file_req("FILE_NOW")`, parse date field from `FILE_NOW_RESULT`.
- `os.time()` calls → parse unix-ts field from `FILE_NOW_RESULT`.
- `os.time({year,month,day,...})` for last_ts → `file_req("FILE_TIMESTAMP <date>")`.
- `os.difftime(a, b)` → plain `a - b` (both are now integer unix timestamps).

### files.c

New C program; reads stdin line by line, ignores non-`FILE_` lines.
For each recognized request, performs the OS operation and writes one response.

```
FILE_LIST_SEQ       glob seq/*.txt, extract numbers, sort, emit space-separated
FILE_LIST_CHN       glob chn/*.txt, extract hashes, emit space-separated
FILE_READ_SEQ <n>   fread seq/<n>.txt, base64-encode, emit FILE_CONTENT
FILE_READ_CHN <h>   fread chn/<h>.txt, base64-encode, emit FILE_CONTENT
FILE_STORE_CHN <b>  decode b64 → buf; SHA-1(buf) → hash;
                      if chn/<hash>.txt absent: write it, emit FILE_STORED <hash>
                      if present and identical:  emit FILE_STORED <hash>
                      if present and different:  emit FILE_COLLISION <hash>
FILE_READ_STATS     fread arg[1] (stats path), base64-encode, emit FILE_CONTENT
FILE_WRITE_STATS    decode b64, write to arg[1] (atomic: write tmp then rename),
                    emit FILE_WRITE_OK
FILE_NOW            time(NULL) → ts; strftime → YYYY-MM-DD;
                    emit FILE_NOW_RESULT <ts> <date>
FILE_TIMESTAMP <d>  parse YYYY-MM-DD with mktime at noon UTC;
                    emit FILE_TIMESTAMP_RESULT <ts>
```

`files.c` takes the stats file path as `argv[1]` (same as stats.lua currently
takes it via `arg[1]`).  `FILE_WRITE_STATS` writes atomically: write to a
`.tmp` file then `rename()` over the target.

### play.dot additions

```
lua src/all.lua         -> src/files.c;
src/files.c             -> lua src/all.lua;
lua src/stats.lua       -> src/files.c;
src/files.c             -> lua src/stats.lua;
```

`files.c` receives merged input from all.lua and stats.lua (run.c merges them);
its stdout is broadcast back to both (run.c fans out).  Since each program
blocks until its own FILE_ response arrives, requests from all.lua and
stats.lua never interleave in practice.
