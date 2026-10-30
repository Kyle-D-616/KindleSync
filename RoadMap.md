# kindlesync

Watch the Jellyfin books folder and email every new DRM-free EPUB to my
`@kindle.com` address, so it lands in my Kindle library via Send to Kindle.

Goal: learn C by building a tool I actually use. No libraries beyond libc.

---

## How it works

```
books folder ──inotify──▶ kindlesync ──pipe──▶ msmtp ──TLS/SMTP──▶ Gmail
                              │                                     │
                     zip DRM check                          relays to Amazon
                     sent log                                       │
                     build MIME email                         Kindle library
```

- **kindlesync** (my C program): finds new EPUBs, skips DRM'd ones,
  builds the full email (headers + MIME + base64 attachment), writes it
  into msmtp's stdin.
- **msmtp**: handles TCP, TLS, SMTP login, and delivery. Exit code tells
  me if it worked.
- **Gmail**: trusted sending server with SPF/DKIM, so Amazon accepts it.

---

## Code style (Ryan Fleury / Casey Muratori)

- **Structs + functions.** No OOP. Data is plain structs.
- **Start in `main`, extract later.** Only pull code into a function after
  writing the same thing twice (Casey's "compression-oriented programming").
- **Unity build.** `main.c` `#include`s every other `.c` file. One compiler
  call in `build.sh`. No Makefile.
- **Arenas over scattered malloc/free.** Allocate a big block, bump a
  pointer, free everything at once. Learn plain malloc/free first.
- **Length-based strings.** `String8 { u8 *str; u64 size; }`. Convert to
  null-terminated only when calling libc/syscalls.
- **Layers + prefixes.** `base_` / `arena_` / `str8_` / `os_` / `zip_` /
  `mail_`. C has no namespaces; prefixes are the namespace.

### Target layout (grow into it, don't start here)

```
kindlesync/
  build.sh
  ROADMAP.md
  src/
    main.c              entry point, #includes everything below
    base/
      base_types.h      u8, u32, s64, b32 typedefs
      base_arena.c      arena allocator
      base_string.c     String8 + helpers
    os/
      os_linux.c        dir listing, file read, inotify, fork/exec
    zip.c               DRM check
    mail.c              MIME + base64
```

---

## Setup

- [ ] Compile flags (catch memory bugs immediately):
      `gcc -Wall -Wextra -g -fsanitize=address,undefined src/main.c -o kindlesync`
- [ ] Put that line in `build.sh`, `chmod +x build.sh`
- [x] Install tools: `gdb`, `man-pages`
- [x] Install `msmtp`
- [ ] Gmail: enable 2FA, generate an **app password**
- [ ] msmtp config at `~/.config/msmtp/config`:
      host, port 465, tls on, auth on, user, `passwordeval`, `logfile`
- [ ] Test msmtp by sending myself a plain-text email from the terminal
- [ ] Amazon: add Gmail address to Approved Personal Document E-mail List
      (Manage Your Content and Devices → Preferences → Personal Document
      Settings)

---

## Roadmap

### 1. List EPUBs in a folder
Take a path as `argv[1]`, print the full path of every `.epub` in it.

- [ ] Check `argc` before touching `argv[1]`
- [ ] `opendir` / `readdir` / `closedir`
- [ ] Check the name ends in `.epub`
- [ ] Build full path with `snprintf`

Learn: strings as char arrays, null terminator, pointer arithmetic.
Man: `opendir(3)`, `readdir(3)`, `snprintf(3)`, `strlen(3)`

### 2. DRM check (parse the zip by hand)
An EPUB is a zip. Read its file list and flag any containing:
`META-INF/rights.xml` (Adobe), `META-INF/sinf.xml` (Apple),
`META-INF/license.lcpl` (Readium LCP).
`encryption.xml` alone is NOT DRM (font obfuscation).

- [ ] `fopen` in binary mode, `fseek`/`ftell` to get file size
- [ ] Find End of Central Directory record (signature `0x06054b50`)
      near the end of the file
- [ ] Read central directory offset + entry count from it
- [ ] Walk central directory entries (signature `0x02014b50`),
      read each filename
- [ ] First `malloc`/`free` — buffer for the file data

Learn: binary I/O, little-endian, structs, heap memory.
Ref: PKWARE APPNOTE.TXT (the zip spec), sections on central directory.
Man: `fopen(3)`, `fread(3)`, `fseek(3)`, `malloc(3)`

### 3. Sent log
Plain text file, one sent path per line. Skip anything already in it.

- [ ] Read whole file into memory, search lines
- [ ] Append new path after a successful send
- [ ] Decide where it lives (`~/.local/state/kindlesync/sent.log`)

Learn: file reading/appending, string searching.

### 4. Build the email
Headers + MIME multipart body + base64-encoded EPUB.

- [ ] Write a base64 encoder (good self-contained exercise)
- [ ] Headers: From, To, Subject, MIME-Version, Content-Type
- [ ] multipart/mixed with a boundary string, one attachment part
- [ ] Lines end in `\r\n`; base64 wrapped at 76 chars
- [ ] Size the output buffer correctly (base64 is 4/3 the input)
- [ ] Write to a file first and inspect it before sending anything

Learn: buffer sizing, building large strings, memory management.
Ref: RFC 2045 (MIME + base64), RFC 5322 (message format).

### 5. Send via msmtp
- [ ] `pipe()` to get read/write fds
- [ ] `fork()`; in the child `dup2` the read end onto stdin, `execvp` msmtp
- [ ] In the parent, write the email to the pipe, close it
- [ ] `waitpid` and check msmtp's exit status
- [ ] Send to my own address first, then `@kindle.com`

Learn: processes, file descriptors, how shells run programs.
Man: `pipe(2)`, `fork(2)`, `dup2(2)`, `execvp(3)`, `waitpid(2)`

### 6. Watch the folder
- [ ] `inotify_init1`, `inotify_add_watch` on the books folder
- [ ] React to `IN_CLOSE_WRITE` and `IN_MOVED_TO` (not `IN_CREATE`,
      or I'll send half-copied files)
- [ ] Parse the variable-length `struct inotify_event` buffer
- [ ] On startup, also scan the folder once for anything missed

Learn: blocking reads, event loops, variable-size structs.
Man: `inotify(7)`, `inotify_add_watch(2)`

### 7. Run it as a service
- [ ] systemd user unit at `~/.config/systemd/user/kindlesync.service`
- [ ] `systemctl --user enable --now kindlesync`
- [ ] Logs via `journalctl --user -u kindlesync`

---

## Refactor milestones (when the pain shows up)

- [ ] Tired of `strlen`/copying → write `String8` in `base_string.c`
- [ ] Tired of tracking `free` → write the arena in `base_arena.c`
- [ ] Linux calls piling up in main → move them to `os/os_linux.c`
- [ ] Zip/mail code is self-contained → move to `zip.c` / `mail.c`

---

## Background notes

**TCP**: reliable, ordered byte stream between two IP:port endpoints,
built on top of unreliable IP packets. Handshake SYN / SYN-ACK / ACK.

**TLS**: encrypts a TCP stream, authenticates the server via certificate,
detects tampering. Port 465 = TLS from the start, 587 = STARTTLS upgrade.
Never write it myself — msmtp handles it.

**SMTP**: text commands over the connection. `EHLO`, `AUTH`,
`MAIL FROM`, `RCPT TO`, `DATA`, `.`, `QUIT`. Replies 2xx ok, 3xx
continue, 4xx temporary fail, 5xx permanent fail. The envelope
(`MAIL FROM`/`RCPT TO`) is what servers route on; `From:`/`To:` headers
are just display text. My program only produces what goes after `DATA`.

**Why Gmail and not direct**: Spectrum blocks outbound port 25,
residential IPs are blocklisted, and I'd need my own domain with
SPF/DKIM. Gmail already has the reputation and signatures.

**Send to Kindle**: books go to the Amazon account library, not a single
device. 50 MB total per email. DRM-free only. Sender must be approved.

---

## Resources

- Ryan Fleury — "Untangling Lifetimes: The Arena Allocator" (Digital Grove
  Substack); RAD Debugger source on GitHub for layer structure
- Casey Muratori — "Semantic Compression" blog post; Handmade Hero
  days 1–5 for build setup
- PKWARE APPNOTE.TXT — zip file format
- RFC 2045, RFC 5322 — MIME and message format
- `man 7 inotify`
