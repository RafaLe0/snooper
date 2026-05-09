# MEMSCAN — Forensic Keyword Scanner

A command-line forensic tool written in C that scans raw binary image files (e.g. USB stick dumps) for keywords and reports every match with its exact byte offset and surrounding context.

---

## Installation

### Requirements

| Tool | Purpose |
|------|---------|
| GCC (any recent version) | Compile `memscan.c` |
| GNU Make | Run build targets |
| Python 3 | Generate the test image (`make_test_image.py`) |

Install on Debian/Ubuntu:

```bash
sudo apt install gcc make python3
```

Install on Arch Linux:

```bash
sudo pacman -S gcc make python
```

### Build

```bash
make
```

This compiles `memscan.c` into `./memscan` with `-Wall -Wextra -g`.

To verify the build is clean (zero warnings):

```bash
gcc -Wall -Wextra -pedantic -std=c11 -g -o memscan memscan.c
```

---

## Usage

```
./memscan <image> <wordlist> [--context N] [--ignore-case] [--reverse]
```

| Argument        | Description                                                        |
|-----------------|--------------------------------------------------------------------|
| `<image>`       | Path to the binary image file to scan                              |
| `<wordlist>`    | Path to the plain-text keyword file (one keyword per line)         |
| `--context N`   | Number of context bytes to capture around each match (default: 16) |
| `--ignore-case` | Enable case-insensitive matching                                   |
| `--reverse`     | Print matches in chronological order (lowest offset first)         |

### Examples

```bash
# Basic scan
./memscan disk.img wordlist.txt

# 32 bytes of context
./memscan disk.img wordlist.txt --context 32

# Case-insensitive (matches "Password", "PASSWORD", "password", etc.)
./memscan disk.img wordlist.txt --ignore-case

# Print matches from lowest offset to highest
./memscan disk.img wordlist.txt --reverse

# All options combined
./memscan disk.img wordlist.txt --context 8 --ignore-case --reverse
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

### Acceptance criteria

- All 12 fragments are found at the correct offsets.
- `valgrind --leak-check=full` reports zero leaks.
- `gcc -Wall -Wextra -pedantic` produces zero warnings.

### Test case-insensitive search

```bash
echo "PASSWORD" > /tmp/ci_test.txt
./memscan usb.img /tmp/ci_test.txt --ignore-case
```

Expected: match found at `0x002200` even though the image contains lowercase `password`.

### Test reverse traversal

```bash
./memscan usb.img wordlist.txt --reverse
```

Expected: matches printed from the lowest offset to the highest (chronological order), without sorting the list — achieved by walking `prev` pointers from the tail back to the head.

### Check for memory leaks (Valgrind)

```bash
valgrind --leak-check=full --track-origins=yes ./memscan usb.img wordlist.txt
```

Expected final line:

```
All heap blocks were freed -- no leaks are possible
```

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

## Exercise Q&A

### Part 1 — File I/O

#### Exercise 1.2 — Questions on chunk reading

**a. Why is the buffer declared as `unsigned char *` rather than `char *`?**

Car nous ne voulons pas des valeurs négatives, mettre unsigned nous assure que les bytes seront interprétés avec leur valeur positive. 

**b. What does `fread` return when it reaches the end of the file?**

`fread` retourne 0 quand il atteind la fin du fichier

**c. What would happen if you used `fopen(path, "r")` instead of `"rb"` on Windows?**

Sur windows, le mode "r" tranforme `\r\n` en `\n`. Ce qui va donc corrompre les offsets et renvoyer les mauvais indices.

---

### Part 2 — Linked Lists

#### Exercise 2.1 — Head insertion: list state diagram

List before inserting `"secret"`:

```
head
 |
 v
[ "passwd" | next ]──►[ "admin" | NULL ]
```

List after inserting `"secret"` at the head:

```
head
 |
 v
[ "secret" | next ]──►[ "passwd" | next ]──►[ "admin" | NULL ]
```

**Why is head insertion O(1) and tail insertion O(N)?**

Head insertion require l'usage de 2 pointers (le nouveau node's `next` and l'update `head`). Tail insertion au contraire, va parcourir tout le node jusqu'au dernier pour ajouter le nouveau a la fin.

**When to prefer one over the other?**

- Si on s'en moque de l'ordre et qu'on veut quelque chose de plus rapide, on va préferer la **head insertion** .
- On preferera la **tail insertion** quand on veut préserver l'ordre.

#### Exercise 2.3 — Pitfall: why you cannot `free(head); head = head->next;`

SI on`free(head)`, la mémoire au niveau de`head` est libérée. Du coup, on ne va plus pouvoir accéder au suivant (`head->next`) car on a perdu l'acces a la zone mémoire

```c
KeywordNode *temp = current;
current = current->next;   // advance first
free(temp);                // then free the saved pointer
```

#### Exercise 2.5 — Why each MatchNode owns its own copy of context bytes

`scan_image` lis les images sur 1MB de chunk. Et on relis les nouveaux chunks a chaque itération. Ainsi, si on ne stocke pas une copie, dès qu'on va re-call fread, on va écraser les valeurs stockés.

---

### Part 3 — Pointers and Memory Management

#### Exercise 3.2 — Byte scanner questions

**a. What does `end - ptr` compute?**

`end` et lié a `buf + bytes_read` représente l'adrsse du dernier byte. `end - ptr` donne le numbre de bytes restants dans le buffer qui reste, en partant du ptr. 

**b. Which function compares raw bytes without assuming a null terminator?**

`memcmp(const void *a, const void *b, size_t n)` compare `n` bytes en partant de `a` et `b`. Il return `0` si tous les`n` bytes sont identiques.

**c. Why does `file_pos + (ptr - buf)` give the correct absolute offset?**

`file_pos` est mis a jour après chaque `fread`. `ptr - buf` c'est l'offset du byte actuel dans le chunk.

#### Exercise 3.3 — Context byte capture questions

**a. Why allocate `ctx_len + 1` bytes instead of `ctx_len`?**

C'est pour s'assurer qu'on termine par un byte NULL, pour pouvoir le passer de maniere sécurisée à toute fonction qui attend une string.

**b. What does `ptr - before_len` point to? Diagram for `ctx_len = 4`, match `"pass"` at position 10:**

```
index:  0    1    2    3    4    5    6    7    8    9   10   11   12   13
buf:  [ ..   ..   ..   ..   ..   ..   p    r    e    f   'p' 'a' 's' 's' ]
                                                          ^
                                                         ptr (position 10)

before_len = min(ctx_len=4, ptr-buf=10) = 4
ptr - before_len = &buf[6]  →  captures bytes [6..9] = "pref"
```

**c. What could go wrong without the `left >= ctx_len` check?**

Si le `ptr` est proche du début du buffer et que`ctx_len` est égal à 16, alors`ptr - ctx_len` va référencer un pointer avant le buffer. Ca créé donc un buffer underflow.

#### Exercise 3.4 — Pointer vs. array notation equivalences

| Pointer notation | Array notation |
|-----------------|----------------|
| `*(buf + i)`    | `buf[i]`       |
| `*(ptr - 3)`    | `ptr[-3]`      |
| `*(end - 1)`    | `end[-1]`      |
| `ptr[0]`        | `*ptr`         |

Si `ptr` est de type`unsigned char *`, alors`ptr + 1` est aussi de type`unsigned char *`.

#### Exercise 3.5 — Memory management audit

| Allocation          | Allocated in   | Freed by        |
|---------------------|----------------|-----------------|
| Read buffer (`buf`) | `scan_image()` | `scan_image()`  |
| Each `KeywordNode`  | `keyword_push()` | `keyword_free()` |
| Each `MatchNode`    | `match_push()` | `match_free()`  |
| `node->context_before` | `match_push()` | `match_free()` |
| `node->context_after`  | `match_push()` | `match_free()` |

**a. What is a memory leak?**

A memory leak c'est quand nous allouons un `malloc` pour une nouvelle adresse mémoire et que nous n'utilisons jamais la fonction `free` pour libérer la mémoire. Le programme perd alors le pointer vers l'emplacement de la mémoire.

**b. What is a use-after-free error?**
C'est quand on essaye d'accéder a un emplacement mémoire qui à déjà été free. L'adresse ne nous appartient donc plus.

**c. What is a buffer overflow? Where could one occur in memscan?**

Un buffer overflow se passe quand on essaye d'écrire apres la fin de l'emplacement mémoire alloué. Dans`memscan`, on obtient un buffer overflow si le check de `remaining < klen` était enlevé : `memcmp` aurait lu le `buf` dans la partie non allouée de la mémoire.

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

## Data structures

### `KeywordNode`

Singly linked list node holding one keyword to search for.

```c
typedef struct KeywordNode {
    char               word[256];
    struct KeywordNode *next;
} KeywordNode;
```

### `MatchNode`

Doubly linked list node holding one match found in the image.

```c
typedef struct MatchNode {
    char             keyword[256];
    long             offset;
    char            *context_before;
    char            *context_after;
    int              context_len;
    struct MatchNode *prev;
    struct MatchNode *next;
} MatchNode;
```

Each `MatchNode` owns **three heap allocations**: itself, `context_before`, and `context_after`. All three are freed by `match_free()`.

The list uses **head insertion**: the most recently found match is at the head, the first found (lowest offset) is at the tail. The `prev` pointer enables reverse traversal without sorting.

---

## Function reference

### Keyword list functions

#### `KeywordNode *keyword_push(KeywordNode *head, const char *word)`
Allocates a new `KeywordNode`, copies `word` into it with `strncpy`, and inserts it at the **head** of the list (O(1)). Returns the new head.

#### `void keyword_print(const KeywordNode *head)`
Walks the list and prints each keyword to stdout, one per line, prefixed with `  - `.

#### `void keyword_free(KeywordNode *head)`
Walks the list and frees every node. Saves `next` before each `free()` to avoid use-after-free.

#### `KeywordNode *keyword_load(const char *filename)`
Opens a plain-text file and builds the keyword linked list from it. Skips blank lines and lines starting with `#`. Strips trailing `\r` and `\n`. Prints a summary line on completion. Returns the head of the list.

---

### Match list functions

#### `MatchNode *match_push(MatchNode *head, const char *keyword, long offset, const char *context_before, const char *context_after, int ctx_len)`
Allocates a new `MatchNode` and fills all its fields. Separately allocates `ctx_len + 1` bytes for `context_before` and `context_after`, zeroes them with `memset`, then copies the raw bytes with `memcpy`. Sets `node->prev = NULL`, links `node->next = head`, and back-links `head->prev = node` when a previous head exists. Returns the new head.

#### `void match_print_report(const MatchNode *head)`
Iterates the match list **from head to tail** (most recently found first) and prints a formatted report for each entry: match number, keyword, absolute offset in hex and decimal, and context bytes with non-printable characters replaced by `.`.

#### `void match_print_reverse(const MatchNode *head)`
Prints matches in **chronological order** (lowest offset first) without sorting the list. Walks forward once to reach the tail, then follows `prev` pointers back to the head. Produces the same formatted output as `match_print_report`. Activated by the `--reverse` flag.

#### `void match_free(MatchNode *head)`
Iterates the match list and frees all three heap allocations per node (`context_before`, `context_after`, then the node itself) in the correct order.

---

### Image I/O functions

#### `FILE *open_image(const char *filename)`
Opens the file in **binary read mode** (`"rb"`). Uses `fseek(SEEK_END)` + `ftell` to measure the file size, then `rewind` to return to the start. Prints the file name and size. Returns the open `FILE *`.

#### `int memcmp_ci(const unsigned char *a, const unsigned char *b, size_t n)`
Case-insensitive byte comparison. Applies `tolower()` to both sides before comparing each byte. Returns 0 on match, non-zero otherwise. Used instead of `memcmp` when `--ignore-case` is active.

#### `void scan_image(FILE *fp, KeywordNode *keywords, MatchNode **matches, int ctx_len, int ignore_case)`
Core scanner. Allocates a 1 MB buffer (`CHUNK_SIZE`) and reads the image chunk by chunk with `fread`. For every byte position in each chunk, tests all keywords using `memcmp` (or `memcmp_ci`). On a match, computes the absolute offset as `file_pos + (ptr - buf)`, clamps context to available bytes, and calls `match_push` to record the result. Tracks `file_pos` across chunks to maintain correct absolute offsets.
