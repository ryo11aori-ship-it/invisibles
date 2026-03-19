#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MEM_SIZE 30000
#define CODE_SIZE 100000

// ===== Unicode codepoints =====
#define U_ZENKAKU_SPACE 0x3000  // >
#define U_EN_QUAD       0x2000  // <
#define U_EM_QUAD       0x2001  // +
#define U_EN_SPACE      0x2002  // -
#define U_EM_SPACE      0x2003  // .
#define U_THREE_PER_EM  0x2004  // ,
#define U_FOUR_PER_EM   0x2005  // [
#define U_SIX_PER_EM    0x2006  // ]
#define U_FIGURE_SPACE  0x2007  // NOP

// ===== 命令列 =====
char code[CODE_SIZE];
int jump_map[CODE_SIZE];
int code_len = 0;

// ===== UTF-8 デコード =====
int read_utf8(FILE *fp, uint32_t *out) {
    int c = fgetc(fp);
    if (c == EOF) return 0;

    if ((c & 0x80) == 0) {
        *out = c;
        return 1;
    }

    int extra = 0;
    if ((c & 0xE0) == 0xC0) extra = 1;
    else if ((c & 0xF0) == 0xE0) extra = 2;
    else if ((c & 0xF8) == 0xF0) extra = 3;
    else return -1;

    uint32_t val = c & (0x7F >> extra);

    for (int i = 0; i < extra; i++) {
        int cc = fgetc(fp);
        if ((cc & 0xC0) != 0x80) return -1;
        val = (val << 6) | (cc & 0x3F);
    }

    *out = val;
    return 1;
}

// ===== コード変換 =====
void load_code(FILE *fp) {
    uint32_t cp;

    while (read_utf8(fp, &cp)) {
        char op = 0;

        if (cp == U_ZENKAKU_SPACE) op = '>';
        else if (cp == U_EN_QUAD) op = '<';
        else if (cp == U_EM_QUAD) op = '+';
        else if (cp == U_EN_SPACE) op = '-';
        else if (cp == U_EM_SPACE) op = '.';
        else if (cp == U_THREE_PER_EM) op = ',';
        else if (cp == U_FOUR_PER_EM) op = '[';
        else if (cp == U_SIX_PER_EM) op = ']';
        else if (cp == U_FIGURE_SPACE) op = 'n'; // nop

        if (op) {
            if (code_len >= CODE_SIZE) {
                fprintf(stderr, "Code too large\n");
                exit(1);
            }
            code[code_len++] = op;
        }
    }
}

// ===== ジャンプテーブル =====
void build_jump_map() {
    int stack[CODE_SIZE];
    int sp = 0;

    for (int i = 0; i < code_len; i++) {
        if (code[i] == '[') {
            stack[sp++] = i;
        } else if (code[i] == ']') {
            if (sp == 0) {
                fprintf(stderr, "Unmatched ]\n");
                exit(1);
            }
            int j = stack[--sp];
            jump_map[i] = j;
            jump_map[j] = i;
        }
    }

    if (sp != 0) {
        fprintf(stderr, "Unmatched [\n");
        exit(1);
    }
}

// ===== 実行 =====
void execute() {
    unsigned char mem[MEM_SIZE] = {0};
    int ptr = 0;
    int pc = 0;

    while (pc < code_len) {
        switch (code[pc]) {
            case '>': ptr++; break;
            case '<': ptr--; break;
            case '+': mem[ptr]++; break;
            case '-': mem[ptr]--; break;
            case '.': putchar(mem[ptr]); break;
            case ',': mem[ptr] = getchar(); break;
            case '[':
                if (mem[ptr] == 0) pc = jump_map[pc];
                break;
            case ']':
                if (mem[ptr] != 0) pc = jump_map[pc];
                break;
            case 'n':
                break;
        }
        pc++;
    }
}

// ===== main =====
int main(int argc, char *argv[]) {
    FILE *fp = stdin;

    if (argc > 1) {
        fp = fopen(argv[1], "rb");
        if (!fp) {
            perror("fopen");
            return 1;
        }
    }

    load_code(fp);
    if (fp != stdin) fclose(fp);

    build_jump_map();
    execute();

    return 0;
}