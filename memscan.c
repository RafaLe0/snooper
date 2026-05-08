#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define CHUNK_SIZE    (1024 * 1024)
#define CTX_LEN       16
#define MAX_WORD_LEN  256

typedef struct KeywordNode {
    char word[MAX_WORD_LEN];
    struct KeywordNode *next;
} KeywordNode;

typedef struct MatchNode {
    char keyword[MAX_WORD_LEN];
    long offset;
    char *context_before;
    char *context_after;
    int context_len;
    struct MatchNode *next;
} MatchNode;


static void print_banner(void)
{
    printf("\n");
    printf("             _,--,            _\n");
    printf("        __,-'____| ___      /' |\n");
    printf("      /'   `\\,--,/'   `\\  /'   |\n");
    printf("     (       )  (       )'\n");
    printf("      \\_   _/'  `\\_   _/\n");
    printf("         \"\"\"        \"\"\"\n");
    printf("⠀⢀⣠⣄⡀⠀⠀⠀⣠⣶⣾⣿⣿⣶⣦⣴⣾⣿⣿⣷⣦⣄⠀⠀⠀⢀⣠⣄⡀⠀\n");
    printf("⣰⣿⠟⠛⢻⡆⣠⣾⣿⣿⣿⣿⣿⣿⡿⣿⣿⣿⣿⣿⣿⣿⣷⡄⢰⠟⠛⢻⣿⡆\n");
    printf("⢻⣿⣦⣀⣤⣾⣿⣿⣿⣿⣿⣿⠟⠋⠀⠀⠙⠿⣿⣿⣿⣿⣿⣿⣦⣤⣀⣼⣿⡇\n");
    printf("⠀⠛⠿⢿⣿⣿⡿⠿⠟⠛⠉⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠿⠿⢿⣿⣿⡿⠿⠋⠀\n");
    printf("\n");
    printf("   M E M S C A N\n");
    printf("   Forensic Keyword Scanner  —  Practical C Project\n");
    printf("   File I/O · Linked Lists · Pointers & Memory Management\n");
    printf("   ────────────────────────────────────────────────────────\n");
    printf("\n");
}


KeywordNode *keyword_push(KeywordNode *head, const char *word)
{
    KeywordNode *node = malloc(sizeof(KeywordNode));
    if (!node) { 
      exit(EXIT_FAILURE); 
    }
    strncpy(node->word, word, MAX_WORD_LEN - 1);
    node->word[MAX_WORD_LEN - 1] = '\0';
    node->next = head;
    return node;
}

void keyword_print(const KeywordNode *head)
{
    const KeywordNode *current = head;
    while (current != NULL) {
        printf("  - %s\n", current->word);
        current = current->next;
    }
}

void keyword_free(KeywordNode *head)
{
    KeywordNode *current = head;
    while (current != NULL) {
        KeywordNode *temp = current;
        current = current->next;
        free(temp);
    }
}

KeywordNode *keyword_load(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror(filename); exit(EXIT_FAILURE); }

    KeywordNode *head  = NULL;
    char         line[MAX_WORD_LEN];
    int          count = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *nl = strpbrk(line, "\r\n");
        if (nl) *nl = '\0';

        if (line[0] == '\0' || line[0] == '#') continue;

        head = keyword_push(head, line);
        count++;
    }

    printf("[*] Loaded %d keyword(s) from '%s'\n", count, filename);
    fclose(fp);
    return head;
}


MatchNode *match_push(MatchNode *head, const char *keyword, long offset, const char *context_before, const char *context_after, int ctx_len)
{
    MatchNode *node = malloc(sizeof(MatchNode));
    if (!node) { 
      exit(EXIT_FAILURE); 
    }

    strncpy(node->keyword, keyword, MAX_WORD_LEN - 1);
    node->keyword[MAX_WORD_LEN - 1] = '\0';
    node->offset = offset;
    node->context_len = ctx_len;

    node->context_before = malloc(ctx_len + 1);
    node->context_after  = malloc(ctx_len + 1);
    if (!node->context_before || !node->context_after) {
      exit(EXIT_FAILURE); 
    }

    memset(node->context_before, 0, ctx_len + 1);
    memset(node->context_after,  0, ctx_len + 1);
    memcpy(node->context_before, context_before, ctx_len);
    memcpy(node->context_after,  context_after,  ctx_len);

    node->next = head;
    return node;
}

void match_print_report(const MatchNode *head)
{
    int count = 1;
    const MatchNode *current = head;
    while (current != NULL) {
        printf("[MATCH #%d]\n", count);
        printf("  Keyword : \"%s\"\n", current->keyword);
        printf("  Offset  : 0x%08lX  (%ld bytes from start)\n", current->offset, current->offset);

        printf("  Context : ");
        for (int i = 0; i < current->context_len; i++) {
            char c = current->context_before[i];
            printf("%c", isprint(c) ? c : '.');
        }
        printf("[%s]", current->keyword);
        for (int i = 0; i < current->context_len; i++) {
            char c = current->context_after[i];
            printf("%c", isprint(c) ? c : '.');
        }
        printf("...\n\n");

        current = current->next;
        count++;
    }
}

void match_free(MatchNode *head)
{
    MatchNode *current = head;
    while (current != NULL) {
        MatchNode *temp = current;
        current = current->next;
        free(temp->context_before);
        free(temp->context_after);
        free(temp);
    }
}


FILE *open_image(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
      exit(EXIT_FAILURE); 
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    printf("[*] Image: '%s'  size: %ld bytes (%ld MB)\n", filename, size, size / (1024 * 1024));
    return fp;
}

int memcmp_ci(const unsigned char *a, const unsigned char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int diff = tolower(a[i]) - tolower(b[i]);
        if (diff != 0) return diff;
    }
    return 0;
}

void scan_image(FILE *fp, KeywordNode *keywords, MatchNode **matches, int ctx_len, int ignore_case)
{
    unsigned char *buf = malloc(CHUNK_SIZE);
    if (!buf) { 
      exit(EXIT_FAILURE); 
    }

    size_t bytes_read;
    long   file_pos = 0;

    while ((bytes_read = fread(buf, 1, CHUNK_SIZE, fp)) > 0) {
        const unsigned char *end = buf + bytes_read;

        for (const unsigned char *ptr = buf; ptr < end; ptr++) {
            size_t remaining = end - ptr;

            for (KeywordNode *kw = keywords; kw != NULL; kw = kw->next) {
                size_t klen = strlen(kw->word);
                if (remaining < klen) continue;

                int match = ignore_case
                    ? memcmp_ci(ptr, (const unsigned char *)kw->word, klen)
                    : memcmp(ptr, kw->word, klen);
                if (match == 0) {
                    long offset = file_pos + (ptr - buf);
                    printf("  [+] Found \"%s\" at offset 0x%08lX\n", kw->word, offset);

                    int before_len = (ptr - buf >= ctx_len) ? ctx_len : (int)(ptr - buf);
                    int after_len  = (int)(end - (ptr + klen)) >= ctx_len ? ctx_len : (int)(end - (ptr + klen));
                    int safe_len   = before_len < after_len ? before_len : after_len;

                    *matches = match_push(*matches, kw->word, offset,
                                         (char *)(ptr - safe_len),
                                         (char *)(ptr + klen),
                                         safe_len);
                }
            }
        }

        file_pos += (long)bytes_read;
    }

    free(buf);
}


int main(int argc, char *argv[])
{
    print_banner();

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <image> <wordlist> [--context N]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *image_path    = argv[1];
    const char *wordlist_path = argv[2];
    int         ctx_len       = CTX_LEN;

    int ignore_case = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--context") == 0 && i + 1 < argc) {
            ctx_len = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--ignore-case") == 0) {
            ignore_case = 1;
        }
    }

    printf("[*] Context length: %d bytes\n", ctx_len);
    printf("[*] Case-insensitive: %s\n", ignore_case ? "yes" : "no");

    KeywordNode *keywords = keyword_load(wordlist_path);
    printf("\nKeywords loaded:\n");
    keyword_print(keywords);

    printf("\n");
    FILE      *fp      = open_image(image_path);
    MatchNode *matches = NULL;

    printf("[*] Scanning...\n\n");
    scan_image(fp, keywords, &matches, ctx_len, ignore_case);
    fclose(fp);

    printf("\n─────────────────────────────────────────────────────\n\n");
    match_print_report(matches);

    match_free(matches);
    keyword_free(keywords);

    return EXIT_SUCCESS;
}
