# MEMSCAN — Forensic Keyword Scanner

A command-line forensic tool written in C that scans raw binary image files (e.g. USB stick dumps) for keywords and reports every match with its exact byte offset and surrounding context.

---

## Installation

### Requirements

- GCC (any recent version)
- Python 3 (for test image generation)
- GNU Make

### Build

```bash
make
```

This compiles `memscan.c` into the `./memscan` binary with `-Wall -Wextra -g`.

---

## Usage

```
./memscan <image> <wordlist> [--context N] [--ignore-case]
```

| Argument        | Description                                              |
|-----------------|----------------------------------------------------------|
| `<image>`       | Path to the binary image file to scan                    |
| `<wordlist>`    | Path to the plain-text keyword file (one keyword per line) |
| `--context N`   | Number of context bytes to capture around each match (default: 16) |
| `--ignore-case` | Enable case-insensitive matching                         |

### Examples

```bash
# Basic scan
./memscan disk.img wordlist.txt

# 32 bytes of context
./memscan disk.img wordlist.txt --context 32

# Case-insensitive (matches "Password", "PASSWORD", "password", etc.)
./memscan disk.img wordlist.txt --ignore-case

# Both options combined
./memscan disk.img wordlist.txt --context 8 --ignore-case
```

### Sample output

```
[*] Context length: 16 bytes
[*] Case-insensitive: no
[*] Loaded 24 keyword(s) from 'wordlist.txt'

Keywords loaded:
  - encrypted
  - password
  ...

[*] Image: 'disk.img'  size: 10485760 bytes (10 MB)
[*] Scanning...

  [+] Found "password" at offset 0x00002200

─────────────────────────────────────────────────────

[MATCH #1]
  Keyword : "password"
  Offset  : 0x00002200  (8704 bytes from start)
  Context : ................[password]=hunter2........
```

---

## Makefile targets

| Command      | Description                                        |
|--------------|----------------------------------------------------|
| `make`       | Compile `memscan.c` → `./memscan`                 |
| `make image` | Generate `usb.img` via `make_test_image.py`        |
| `make run`   | Compile + generate image + run the scanner         |
| `make clean` | Remove `./memscan` and `usb.img`                   |

---

## Testing

Generate the test image and run a full scan in one command:

```bash
make run
```

The test image (`usb.img`, 2 MB) contains 12 fragments embedded at known offsets:

| Offset     | Fragment                              |
|------------|---------------------------------------|
| `0x001000` | `login: admin`                        |
| `0x002200` | `password=hunter2`                    |
| `0x005000` | `secret: topsecret123`                |
| `0x008400` | `token: eyJhbGciOiJIUzI1NiJ9`         |
| `0x00A000` | `apikey=sk-abc123456789`              |
| `0x010000` | `http://malicious.example.com/payload`|
| `0x015000` | `ssh-rsa AAAAB3NzaC1yc2E...`          |
| `0x020000` | `-----BEGIN RSA PRIVATE KEY-----`     |
| `0x030000` | `user@gmail.com`                      |
| `0x040000` | `CVV: 123  credit_card: 4111...`      |
| `0x050000` | `bitcoin:1A2B3C4D5EF  wallet.dat`     |
| `0x060000` | `ransom: pay now or your files stay encrypted` |

### Test case-insensitive search

```bash
echo "PASSWORD" > /tmp/ci_test.txt
./memscan usb.img /tmp/ci_test.txt --ignore-case
```

Expected: match found at `0x002200` even though the image contains lowercase `password`.

### Wordlist format

```
# Lines starting with '#' are comments — ignored
# Blank lines are also ignored

password
admin
@gmail.com
BEGIN RSA PRIVATE KEY
```

---

## Project structure

```
.
├── memscan.c            # Main source file
├── wordlist.txt         # Default keyword list (24 keywords)
├── make_test_image.py   # Generates usb.img with embedded fragments
├── Makefile             # Build and test shortcuts
└── README.md            # This file
```

---

## Function reference

### Data structures

#### `KeywordNode`
Singly linked list node holding one keyword to search for.

```c
typedef struct KeywordNode {
    char               word[256]; // the keyword string
    struct KeywordNode *next;
} KeywordNode;
```

#### `MatchNode`
Singly linked list node holding one match found in the image.

```c
typedef struct MatchNode {
    char             keyword[256]; // matched keyword
    long             offset;       // absolute byte offset in the file
    char            *context_before; // heap-allocated bytes before the match
    char            *context_after;  // heap-allocated bytes after the match
    int              context_len;
    struct MatchNode *next;
} MatchNode;
```

Each `MatchNode` owns **three heap allocations**: itself, `context_before`, and `context_after`. All three are freed by `match_free()`.

---

### Keyword list functions

#### `KeywordNode *keyword_push(KeywordNode *head, const char *word)`
Allocates a new `KeywordNode`, copies `word` into it, and inserts it at the **head** of the list (O(1)). Returns the new head.

#### `void keyword_print(const KeywordNode *head)`
Walks the list and prints each keyword to stdout, one per line, prefixed with `  - `.

#### `void keyword_free(KeywordNode *head)`
Walks the list and frees every node. Saves `next` before each `free()` to avoid use-after-free.

#### `KeywordNode *keyword_load(const char *filename)`
Opens a plain-text file and builds the keyword linked list from it. Skips blank lines and lines starting with `#`. Strips trailing `\r` and `\n`. Prints a summary line on completion. Returns the head of the list.

---

### Match list functions

#### `MatchNode *match_push(MatchNode *head, const char *keyword, long offset, const char *context_before, const char *context_after, int ctx_len)`
Allocates a new `MatchNode` and fills all its fields. Separately allocates `ctx_len + 1` bytes for `context_before` and `context_after`, zeroes them with `memset`, then copies the raw bytes with `memcpy`. Inserts at the head and returns the new head.

#### `void match_print_report(const MatchNode *head)`
Iterates the match list and prints a formatted report for each entry:
- Match number
- Keyword (quoted)
- Absolute offset in hex and decimal
- Context: raw bytes before and after the match, non-printable characters replaced with `.`

#### `void match_free(MatchNode *head)`
Iterates the match list and frees all three heap allocations per node (`context_before`, `context_after`, then the node itself) in the correct order.

---

### Image I/O functions

#### `FILE *open_image(const char *filename)`
Opens the file in **binary read mode** (`"rb"`). Uses `fseek(SEEK_END)` + `ftell` to measure the file size, then `rewind` to return to the start. Prints the file name and size. Returns the open `FILE *`.

#### `int memcmp_ci(const unsigned char *a, const unsigned char *b, size_t n)`
Case-insensitive byte comparison. Applies `tolower()` to both sides before comparing each byte. Returns 0 on match, non-zero otherwise. Used instead of `memcmp` when `--ignore-case` is active.

#### `void scan_image(FILE *fp, KeywordNode *keywords, MatchNode **matches, int ctx_len, int ignore_case)`
Core scanner. Allocates a 1 MB buffer (`CHUNK_SIZE`) and reads the image chunk by chunk with `fread`. For every byte position in each chunk, tests all keywords using `memcmp` (or `memcmp_ci`). On a match:
- Computes the absolute offset as `file_pos + (ptr - buf)`
- Clamps context to available bytes so reads never go out of bounds
- Calls `match_push` to record the result

Tracks `file_pos` across chunks to maintain correct absolute offsets throughout the file.
