#include <string.h>

#include "front/lexer.h"

typedef struct {
  const char *text;
  size_t len;
  size_t pos;
  int line;
  int col;
  Token *out;
  size_t count;
  Diag *diag;
  const char *source;
} Lexer;

static const struct {
  const char *word;
  TokKind kind;
} KEYWORDS[] = {
  {"def", TOK_KW_DEF},
  {"rec", TOK_KW_REC},
  {"fun", TOK_KW_FUN},
  {"family", TOK_KW_FAMILY},
  {"axiom", TOK_KW_AXIOM},
  {"Type", TOK_KW_TYPE},
  {"Sigma", TOK_KW_SIGMA},
  {"circuit", TOK_KW_CIRCUIT},
  {"public", TOK_KW_PUBLIC},
  {"private", TOK_KW_PRIVATE},
  {"hint", TOK_KW_HINT},
  {"range8", TOK_KW_RANGE8},
  {"mutual", TOK_KW_MUTUAL},
};

const char *tok_kind_name(TokKind kind) {
  switch (kind) {
    case TOK_EOF: return "end of input";
    case TOK_IDENT: return "a name";
    case TOK_NAT: return "a Nat literal";
    case TOK_LPAREN: return "'('";
    case TOK_RPAREN: return "')'";
    case TOK_COLON: return "':'";
    case TOK_DEFINE: return "':='";
    case TOK_ARROW: return "'->'";
    case TOK_FATARROW: return "'=>'";
    case TOK_STAR: return "'*'";
    case TOK_BAR: return "'|'";
    case TOK_KW_DEF: return "'def'";
    case TOK_KW_REC: return "'rec'";
    case TOK_KW_FUN: return "'fun'";
    case TOK_KW_FAMILY: return "'family'";
    case TOK_KW_AXIOM: return "'axiom'";
    case TOK_KW_TYPE: return "'Type'";
    case TOK_KW_SIGMA: return "'Sigma'";
    case TOK_PLUS: return "'+'";
    case TOK_MINUS: return "'-'";
    case TOK_EQEQ: return "'=='";
    case TOK_SEMI: return "';'";
    case TOK_KW_CIRCUIT: return "'circuit'";
    case TOK_KW_PUBLIC: return "'public'";
    case TOK_KW_PRIVATE: return "'private'";
    case TOK_KW_HINT: return "'hint'";
    case TOK_KW_RANGE8: return "'range8'";
    case TOK_LANGLE: return "'<'";
    case TOK_RANGLE: return "'>'";
    case TOK_AT: return "'@'";
    case TOK_TILDE: return "'~'";
    case TOK_MEET: return "'/\\'";
    case TOK_JOIN: return "'\\/'";
    case TOK_LBRACKET: return "'['";
    case TOK_RBRACKET: return "']'";
    case TOK_COMMA: return "','";
    case TOK_KW_MUTUAL: return "'mutual'";
  }
  return "a token";
}

static int is_digit(char c) {
  return c >= '0' && c <= '9';
}

static int is_ident_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(char c) {
  return is_ident_start(c) || is_digit(c) || c == '\'';
}

static char peek(const Lexer *lx, size_t ahead) {
  return lx->pos + ahead < lx->len ? lx->text[lx->pos + ahead] : '\0';
}

static void advance(Lexer *lx, size_t width) {
  for (size_t i = 0; i < width && lx->pos < lx->len; i++) {
    int newline = lx->text[lx->pos] == '\n';
    lx->line += newline;
    lx->col = newline ? 1 : lx->col + 1;
    lx->pos++;
  }
}

static Token *push(Lexer *lx, TokKind kind, size_t start, int line, int col) {
  Token *tok = &lx->out[lx->count];
  lx->count++;
  tok->kind = kind;
  tok->text = lx->text + start;
  tok->len = lx->pos - start;
  tok->nat = 0;
  tok->line = line;
  tok->col = col;
  return tok;
}

/* Skips white space and `--` comments to the end of the line. */
static void skip_trivia(Lexer *lx) {
  for (;;) {
    char c = peek(lx, 0);
    int space = lx->pos < lx->len && (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    int comment = c == '-' && peek(lx, 1) == '-';
    if (!space && !comment) return;
    if (space) {
      advance(lx, 1);
      continue;
    }
    while (lx->pos < lx->len && lx->text[lx->pos] != '\n') advance(lx, 1);
  }
}

static TokKind keyword_kind(const char *text, size_t len) {
  for (size_t i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++) {
    if (strlen(KEYWORDS[i].word) == len && memcmp(KEYWORDS[i].word, text, len) == 0) return KEYWORDS[i].kind;
  }
  return TOK_IDENT;
}

static int lex_ident(Lexer *lx) {
  size_t start = lx->pos;
  int line = lx->line;
  int col = lx->col;
  while (is_ident_char(peek(lx, 0))) advance(lx, 1);
  push(lx, keyword_kind(lx->text + start, lx->pos - start), start, line, col);
  return 1;
}

static int lex_nat(Lexer *lx) {
  size_t start = lx->pos;
  int line = lx->line;
  int col = lx->col;
  uint64_t value = 0;
  int overflow = 0;
  while (is_digit(peek(lx, 0))) {
    uint64_t digit = (uint64_t)(peek(lx, 0) - '0');
    overflow |= value > (UINT64_MAX - digit) / 10u;
    value = value * 10u + digit;
    advance(lx, 1);
  }
  if (is_ident_start(peek(lx, 0))) {
    return diag_fail(lx->diag, "LEX_CHAR", NULL, "%s:%d:%d: a letter follows a Nat literal", lx->source, lx->line, lx->col);
  }
  Token *tok = push(lx, TOK_NAT, start, line, col);
  tok->nat = value;
  tok->big = overflow; /* the checker refuses it as a Nat (LEX_NAT_RANGE) */
  return 1;
}

static int lex_symbol(Lexer *lx) {
  size_t start = lx->pos;
  int line = lx->line;
  int col = lx->col;
  char c = peek(lx, 0);
  char next = peek(lx, 1);
  TokKind kind = TOK_EOF;
  size_t width = 1;
  switch (c) {
    case '(': kind = TOK_LPAREN; break;
    case ')': kind = TOK_RPAREN; break;
    case '*': kind = TOK_STAR; break;
    case '+': kind = TOK_PLUS; break;
    case '|': kind = TOK_BAR; break;
    case ';': kind = TOK_SEMI; break;
    case '<': kind = TOK_LANGLE; break;
    case '>': kind = TOK_RANGLE; break;
    case '@': kind = TOK_AT; break;
    case '~': kind = TOK_TILDE; break;
    case '[': kind = TOK_LBRACKET; break;
    case ']': kind = TOK_RBRACKET; break;
    case ',': kind = TOK_COMMA; break;
    case '/':
      kind = next == '\\' ? TOK_MEET : TOK_EOF;
      width = 2;
      break;
    case '\\':
      kind = next == '/' ? TOK_JOIN : TOK_EOF;
      width = 2;
      break;
    case ':':
      kind = next == '=' ? TOK_DEFINE : TOK_COLON;
      width = next == '=' ? 2 : 1;
      break;
    case '-':
      kind = next == '>' ? TOK_ARROW : TOK_MINUS;
      width = next == '>' ? 2 : 1;
      break;
    case '=':
      kind = next == '>' ? TOK_FATARROW : next == '=' ? TOK_EQEQ : TOK_EOF;
      width = 2;
      break;
    default: break;
  }
  if (kind == TOK_EOF) {
    return diag_fail(lx->diag, "LEX_CHAR", NULL, "%s:%d:%d: unexpected byte 0x%02x", lx->source, line, col, (unsigned)(unsigned char)c);
  }
  advance(lx, width);
  push(lx, kind, start, line, col);
  return 1;
}

int lex_source(Arena *arena, const char *source, const char *text, size_t len, TokenList *out, Diag *diag) {
  /* Each token takes at least one byte, and one EOF token follows. */
  Token *tokens = arena_alloc(arena, (len + 1) * sizeof(Token));
  if (tokens == NULL) return diag_fail(diag, "OOM", NULL, "%s: no memory for the tokens", source);
  Lexer lx = {
    .text = text, .len = len, .pos = 0, .line = 1, .col = 1,
    .out = tokens, .count = 0, .diag = diag, .source = source,
  };
  for (;;) {
    skip_trivia(&lx);
    if (lx.pos >= lx.len) break;
    char c = peek(&lx, 0);
    int ok = is_ident_start(c) ? lex_ident(&lx) : is_digit(c) ? lex_nat(&lx) : lex_symbol(&lx);
    if (!ok) return 0;
  }
  push(&lx, TOK_EOF, lx.pos, lx.line, lx.col);
  out->items = tokens;
  out->count = lx.count;
  return 1;
}
