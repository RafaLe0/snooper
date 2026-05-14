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
[*] Loaded 1432 keyword(s) from 'wordlist.txt'

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

---

## Wordlist

`wordlist.txt` is a plain-text file — one keyword per line. Lines starting with `#` and blank lines are ignored.

```
# Lines starting with '#' are comments — ignored
# Blank lines are also ignored

password
admin
@gmail.com
-----BEGIN RSA PRIVATE KEY-----
```

The bundled `wordlist.txt` contains **1 432 keywords** across 18 forensic categories:

### CTF flag formats

Common bracket-style flag prefixes used across CTF platforms.

| Examples |
|----------|
| `flag{` `FLAG{` `CTF{` `ESGI{` `HTB{` `THM{` `picoCTF{` `utflag{` `sekai{` `corctf{` |

### Authentication & credentials

Credential field names, token types, API key patterns, common weak passwords, and hash prefixes found in memory or config files.

| Sub-category | Examples |
|---|---|
| Field names | `password` `passwd` `secret` `token` `apikey` `credentials` |
| Token types | `JWT` `bearer_token` `access_token` `refresh_token` `oauth_token` |
| API keys | `AWS_SECRET_ACCESS_KEY` `GOOGLE_API_KEY` `client_secret` `signing_key` |
| Weak passwords | `hunter2` `P@ssw0rd` `admin123` `letmein` `qwerty` `123456` |
| Hash prefixes | `$2b$` `$6$` `SHA256:` `MD5:` `PBKDF2` |

### File magic bytes / headers

Binary file signatures and PEM block delimiters that appear as literal strings in raw images.

| Format | Signature |
|---|---|
| JPEG | `JFIF` `Exif` |
| PNG | `PNG` |
| PDF | `%PDF` |
| ZIP / JAR | `PK` |
| PE executable | `MZ` |
| ELF binary | `ELF` |
| RAR | `Rar!` |
| 7-Zip | `7z` |
| RSA private key | `-----BEGIN RSA PRIVATE KEY-----` |
| OpenSSH key | `-----BEGIN OPENSSH PRIVATE KEY-----` |
| PGP message | `-----BEGIN PGP MESSAGE-----` |
| SQLite | `SQLite format 3` |

### SSH & remote access

SSH key types, config keywords, and remote-access tool names.

| Examples |
|----------|
| `ssh-rsa` `ssh-ed25519` `ecdsa-sha2-nistp256` `id_rsa` `authorized_keys` `known_hosts` |
| `putty` `WinSCP` `TeamViewer` `AnyDesk` `RDP` `VNC` |

### Network artifacts

URL schemes, HTTP headers, private IP ranges, common port patterns, session cookie names, and dark-web domains.

| Sub-category | Examples |
|---|---|
| URL schemes | `http://` `https://` `ftp://` `sftp://` `ssh://` `smb://` `.onion` |
| HTTP headers | `Authorization:` `Cookie:` `Set-Cookie:` `X-API-Key:` `X-Forwarded-For:` |
| Private IPs | `192.168.` `10.0.` `172.16.` `127.0.0.1` |
| Session cookies | `PHPSESSID` `JSESSIONID` `ASP.NET_SessionId` `remember_me` |

### Personally identifiable information (PII)

Email domains, payment card fields, banking identifiers, and identity document keywords.

| Sub-category | Examples |
|---|---|
| Email domains | `@gmail.com` `@protonmail.com` `@outlook.com` `@tutanota.com` |
| Payment cards | `credit_card` `CVV` `CVC` `card_number` `expiry` `billing_address` |
| Banking | `IBAN` `SWIFT` `BIC` `bank_account` `routing_number` `stripe_secret` |
| Identity | `SSN` `passport` `drivers_license` `date_of_birth` `national_id` |

### Cryptocurrency

Coin names and tickers, wallet files, seed phrase keywords, HD key prefixes, and exchange names.

| Sub-category | Examples |
|---|---|
| Coins | `bitcoin` `BTC` `ethereum` `ETH` `monero` `XMR` `litecoin` `dogecoin` |
| Wallet files | `wallet.dat` `wallet.json` `keystore` |
| Key material | `mnemonic` `seed_phrase` `xprv` `xpub` `private_key_hex` |
| Exchanges | `coinbase` `binance.com` `blockchain.info` `metamask` `electrum` |

### Malware & threat artifacts

Ransomware note strings, malware type names, C2 framework indicators, and code injection patterns.

| Sub-category | Examples |
|---|---|
| Ransomware | `ransom` `YOUR FILES` `pay now` `HOW_TO_DECRYPT` `README_DECRYPT` `RECOVERY_KEY` |
| Malware types | `backdoor` `trojan` `rootkit` `keylogger` `cryptominer` `RAT` |
| C2 frameworks | `cobalt_strike` `meterpreter` `metasploit` `empire` `covenant` |
| Injection API | `CreateRemoteThread` `VirtualAllocEx` `WriteProcessMemory` `NtUnmapViewOfSection` |
| LOLBins | `certutil -decode` `mshta` `regsvr32` `bitsadmin` `wmic process call create` |
| Obfuscation | `base64_decode(` `gzinflate(` `eval(` `xor_key` `obfuscated` |

### Linux / Unix system artifacts

Sensitive file paths, shell history files, suspicious one-liner patterns.

| Sub-category | Examples |
|---|---|
| Credential files | `/etc/passwd` `/etc/shadow` `/etc/sudoers` |
| History files | `.bash_history` `.zsh_history` `.mysql_history` `.viminfo` |
| SSH material | `.ssh/` `authorized_keys` `id_rsa` `id_ed25519` |
| Suspicious commands | `chmod +x` `base64 -d` `wget http` `curl http` `nc -lvp` `iptables -F` |

### Windows system artifacts

Registry hives, NTFS metadata files, prefetch, event IDs, and LOLBins.

| Sub-category | Examples |
|---|---|
| Registry hives | `NTUSER.DAT` `SAM` `SYSTEM` `SECURITY` `SOFTWARE` |
| NTFS artifacts | `$MFT` `$LogFile` `$Bitmap` `$Recycle.Bin` |
| Execution traces | `prefetch` `amcache` `shimcache` `AppCompatCache` |
| Event IDs | `4624` `4625` `4648` `4672` `4720` `4740` `4776` |
| LOLBins | `powershell -enc` `certutil.exe` `mshta.exe` `rundll32.exe` `wscript.exe` |

### Email artifacts

SMTP/MIME header names and phishing indicator strings.

| Examples |
|----------|
| `From:` `To:` `Subject:` `Reply-To:` `Received: from` `MIME-Version:` `X-Originating-IP:` |
| `phishing` `spear phishing` `whaling` `vishing` `smishing` `mail bomb` |

### Cloud & DevOps secrets

Provider-specific key prefixes, CI/CD tokens, container secrets, and config file names.

| Provider | Examples |
|---|---|
| AWS | `AWS_ACCESS_KEY_ID` `AWS_SECRET_ACCESS_KEY` `AKIA` `arn:aws:` `.aws/credentials` |
| GCP | `GOOGLE_API_KEY` `GOOGLE_APPLICATION_CREDENTIALS` `AIza` `service_account` |
| Azure | `AZURE_CLIENT_SECRET` `AZURE_STORAGE_KEY` `DefaultEndpointsProtocol=https` |
| GitHub / GitLab | `GITHUB_TOKEN` `ghp_` `github_pat_` `GITLAB_TOKEN` `CI_JOB_TOKEN` |
| Docker / K8s | `DOCKER_PASSWORD` `KUBECONFIG` `kubernetes.io/dockerconfigjson` `imagePullSecret` |
| Config files | `.env` `.env.production` `secrets.yaml` `credentials.json` `service-account.json` |

### Database artifacts

SQL DML/DDL keywords, connection string patterns, and database file extensions.

| Sub-category | Examples |
|---|---|
| SQL keywords | `SELECT password` `INSERT INTO users` `UPDATE users SET` `GRANT ALL` |
| Connection strings | `Data Source=` `User ID=` `Password=` `ConnectionString` |
| DB files | `.sql` `.sqlite3` `.db` `.mdb` `mysqldump` `pg_dump` |

### Web application artifacts

PHP dangerous sinks, CMS config files, client-side attack patterns, and vulnerability abbreviations.

| Sub-category | Examples |
|---|---|
| PHP sinks | `shell_exec(` `exec(` `system(` `eval(` `base64_decode(` `$_GET[` `$_POST[` |
| CMS configs | `wp-config.php` `wp-admin` `settings.php` `.htaccess` `.htpasswd` |
| Client-side | `document.cookie` `innerHTML` `localStorage` `XMLHttpRequest` |
| Vuln names | `XSS` `SQLi` `LFI` `RFI` `SSRF` `XXE` `IDOR` `RCE` `CSRF` |

### Encoding & cryptography

Cipher and hash algorithm names, key-derivation schemes, and common base64 header patterns.

| Sub-category | Examples |
|---|---|
| Ciphers | `AES` `RSA` `DES` `ChaCha20` `Blowfish` `RC4` `ROT13` `XOR` |
| Hashes | `MD5` `SHA256` `SHA512` `bcrypt` `scrypt` `Argon2` `HMAC` `CRC32` |
| Key exchange | `Diffie-Hellman` `ECDH` `ECDSA` `PKCS` `OAEP` |
| Base64 prefixes | `eyJ` (JWT) `TVqQ` (PE/MZ) `UEsDB` (ZIP) |

### Steganography artifacts

Tool names, technique keywords, and metadata field names.

| Sub-category | Examples |
|---|---|
| Tools | `steghide` `outguess` `stegsolve` `zsteg` `binwalk` `stegdetect` `SilentEye` |
| Techniques | `LSB` `watermark` `covert channel` `least significant` `hidden message` |
| Metadata | `EXIF` `IPTC` `XMP` `tEXt` `zTXt` `exiftool` `Metadata` |

### Social engineering

Urgency language, impersonation phrases, and lure keywords common in phishing and BEC attacks.

| Examples |
|----------|
| `click here` `verify your account` `urgent action required` `your account has been` |
| `CEO fraud` `BEC (Business Email Compromise)` `gift card` `wire transfer request` |
| `you have won` `lottery` `inheritance` `suspicious login` `security alert` |

### Miscellaneous

Developer breadcrumbs, sensitivity labels, and authentication bypass indicators.

| Sub-category | Examples |
|---|---|
| Dev breadcrumbs | `TODO` `FIXME` `HACK` `HARDCODED` `DO NOT COMMIT` `debug only` `temp password` |
| Sensitivity labels | `CONFIDENTIAL` `CLASSIFIED` `TOP SECRET` `PRIVATE` `PROPRIETARY` |
| MFA / OTP | `OTP` `2FA` `MFA` `TOTP` `HOTP` `authenticator` `backup code` `recovery code` |

---

## Project structure

```
.
├── memscan.c            # Main source file
├── wordlist.txt         # Forensic keyword dictionary (1 432 keywords, 18 categories)
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
