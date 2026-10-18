/* Grammar:
     decl    := 'def' 'rec'? NAME ':' term ':=' term
              | 'axiom' NAME ':' term
              | 'family' NAME group* ':=' ctor ('|' ctor)*
     ctor    := NAME group* ('<' NAME+ '>' system)?   (M4: a path constructor)
     system  := '[' join '->' term (',' join '->' term)* ']'
     group   := '(' NAME+ ':' term ')'
     term    := 'fun' (NAME | group)+ '=>' term
              | group ('->' | '*') term
              | '<' NAME '>' term                (path abstraction)
              | join ('->' term)?
     join    := meet ('\/' meet)*             (interval connections)
     meet    := sum ('/\' sum)*
     sum     := prod (('+' | '-') prod)*      (Field arithmetic)
     prod    := app ('*' app)*
     app     := atom (atom | '@' atom)*       (application, path application)
     atom    := NAME | NAT | 'Type' NAT | 'Sigma' group atom | '(' term ')' | '~' atom */
#include <string.h>

#include "front/parser.h"

#define PARSE_MAX_DEPTH 1000

typedef struct {
  const Token *toks;
  size_t count;
  size_t pos;
  Arena *arena;
  Diag *diag;
  const char *source;
  const char *def;
  Origin origin;
  int depth;
} Parser;

/* One binder group: `(x y : A)` gives two names that share one type. */
typedef struct {
  const char **names;
  size_t count;
  const Term *type;
  const Token *tok;
} Binders;

static const Term *parse_term(Parser *p);
static const Term *parse_atom(Parser *p);
static const Term *parse_join(Parser *p);

static const Token *peek_at(const Parser *p, size_t ahead) {
  size_t i = p->pos + ahead;
  return &p->toks[i < p->count ? i : p->count - 1];
}

static const Token *cur(const Parser *p) {
  return peek_at(p, 0);
}

static int at(const Parser *p, TokKind kind) {
  return cur(p)->kind == kind;
}

static void bump(Parser *p) {
  p->pos += p->pos + 1 < p->count ? 1u : 0u;
}

static int eat(Parser *p, TokKind kind) {
  if (!at(p, kind)) return 0;
  bump(p);
  return 1;
}

static int oom(Parser *p) {
  return diag_fail(p->diag, "OOM", p->def, "%s: the arena limit is reached", p->source);
}

static int fail_at(Parser *p, const Token *tok, const char *what) {
  return diag_fail(p->diag, "PARSE_EXPECT", p->def, "%s:%d:%d: expected %s, found %s",
                   p->source, tok->line, tok->col, what, tok_kind_name(tok->kind));
}

static int expect(Parser *p, TokKind kind, const char *what) {
  return eat(p, kind) ? 1 : fail_at(p, cur(p), what);
}

static int enter(Parser *p) {
  p->depth++;
  if (p->depth <= PARSE_MAX_DEPTH) return 1;
  return diag_fail(p->diag, "PARSE_DEPTH", p->def, "%s:%d:%d: terms nest deeper than %d levels",
                   p->source, cur(p)->line, cur(p)->col, PARSE_MAX_DEPTH);
}

static const char *token_text(Parser *p, const Token *tok) {
  char *text = arena_strndup(p->arena, tok->text, tok->len);
  if (text == NULL) oom(p);
  return text;
}

static const char *take_name(Parser *p, const char *what) {
  if (!at(p, TOK_IDENT)) {
    fail_at(p, cur(p), what);
    return NULL;
  }
  const Token *tok = cur(p);
  bump(p);
  return token_text(p, tok);
}

static Term *make_term(Parser *p, TermKind kind, int line, int col) {
  Term *term = arena_alloc(p->arena, sizeof *term);
  if (term == NULL) {
    oom(p);
    return NULL;
  }
  term->kind = kind;
  term->line = line;
  term->col = col;
  return term;
}

/* Grows an arena array by doubling. The old block stays in the arena. */
static void *grow(Parser *p, void *items, size_t count, size_t *cap, size_t size) {
  if (count < *cap) return items;
  size_t next = *cap == 0 ? 4 : *cap * 2;
  void *fresh = arena_alloc(p->arena, next * size);
  if (fresh == NULL) {
    oom(p);
    return NULL;
  }
  if (count > 0) memcpy(fresh, items, count * size);
  *cap = next;
  return fresh;
}

static int starts_atom(TokKind kind) {
  switch (kind) {
    case TOK_IDENT:
    case TOK_NAT:
    case TOK_LPAREN:
    case TOK_KW_TYPE:
    case TOK_KW_SIGMA:
    case TOK_TILDE:
    case TOK_LBRACKET:
      return 1;
    case TOK_LANGLE:
    case TOK_RANGLE:
    case TOK_AT:
    case TOK_MEET:
    case TOK_JOIN:
    case TOK_RBRACKET:
    case TOK_COMMA:
    case TOK_EOF:
    case TOK_RPAREN:
    case TOK_COLON:
    case TOK_DEFINE:
    case TOK_ARROW:
    case TOK_FATARROW:
    case TOK_STAR:
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_EQEQ:
    case TOK_SEMI:
    case TOK_KW_CIRCUIT:
    case TOK_KW_PUBLIC:
    case TOK_KW_PRIVATE:
    case TOK_KW_HINT:
    case TOK_KW_RANGE8:
    case TOK_KW_MUTUAL:
    case TOK_BAR:
    case TOK_KW_DEF:
    case TOK_KW_REC:
    case TOK_KW_FUN:
    case TOK_KW_FAMILY:
    case TOK_KW_AXIOM:
      return 0;
  }
  return 0;
}

/* `(` NAME+ `:` starts a binder group. `(f x)` does not. */
static int at_binder_group(const Parser *p) {
  if (!at(p, TOK_LPAREN) || peek_at(p, 1)->kind != TOK_IDENT) return 0;
  size_t ahead = 2;
  while (peek_at(p, ahead)->kind == TOK_IDENT) ahead++;
  return peek_at(p, ahead)->kind == TOK_COLON;
}

static int parse_group(Parser *p, Binders *out) {
  out->tok = cur(p);
  if (!expect(p, TOK_LPAREN, "'('")) return 0;
  size_t start = p->pos;
  while (at(p, TOK_IDENT)) bump(p);
  out->count = p->pos - start;
  if (out->count == 0) return fail_at(p, cur(p), "a binder name");
  out->names = arena_alloc(p->arena, out->count * sizeof *out->names);
  if (out->names == NULL) return oom(p);
  for (size_t i = 0; i < out->count; i++) {
    out->names[i] = token_text(p, &p->toks[start + i]);
    if (out->names[i] == NULL) return 0;
  }
  if (!expect(p, TOK_COLON, "':'")) return 0;
  out->type = parse_term(p);
  return out->type != NULL && expect(p, TOK_RPAREN, "')'");
}

/* A bare lambda binder `x` has no type. The checker infers it. */
static int parse_bare(Parser *p, Binders *out) {
  out->tok = cur(p);
  out->names = arena_alloc(p->arena, sizeof *out->names);
  if (out->names == NULL) return oom(p);
  out->names[0] = take_name(p, "a binder");
  out->count = 1;
  out->type = NULL;
  return out->names[0] != NULL;
}

/* Builds the nested binders of B around BODY, the last name innermost. */
static const Term *wrap(Parser *p, TermKind kind, const Binders *b, const Term *body) {
  const Term *acc = body;
  for (size_t i = b->count; i > 0 && acc != NULL; i--) {
    Term *node = make_term(p, kind, b->tok->line, b->tok->col);
    if (node == NULL) return NULL;
    node->name = b->names[i - 1];
    node->left = b->type;
    node->right = acc;
    acc = node;
  }
  return acc;
}

static const Term *parse_lambda_tail(Parser *p) {
  if (eat(p, TOK_FATARROW)) return parse_term(p);
  Binders b;
  int ok = at(p, TOK_IDENT) ? parse_bare(p, &b) : parse_group(p, &b);
  if (!ok || !enter(p)) return NULL;
  const Term *body = parse_lambda_tail(p);
  p->depth--;
  return body == NULL ? NULL : wrap(p, TERM_LAM, &b, body);
}

static const Term *parse_fun(Parser *p) {
  bump(p);
  if (at(p, TOK_FATARROW)) {
    fail_at(p, cur(p), "a binder after 'fun'");
    return NULL;
  }
  return parse_lambda_tail(p);
}

static const Term *parse_binder_type(Parser *p) {
  Binders b;
  if (!parse_group(p, &b)) return NULL;
  int sigma = at(p, TOK_STAR);
  if (!sigma && !at(p, TOK_ARROW)) {
    fail_at(p, cur(p), "'->' or '*' after a binder");
    return NULL;
  }
  bump(p);
  const Term *body = parse_term(p);
  return body == NULL ? NULL : wrap(p, sigma ? TERM_SIGMA : TERM_PI, &b, body);
}

static const Term *parse_universe(Parser *p) {
  const Token *tok = cur(p);
  bump(p);
  if (!at(p, TOK_NAT)) {
    fail_at(p, cur(p), "a universe level after 'Type'");
    return NULL;
  }
  Term *term = make_term(p, TERM_UNIV, tok->line, tok->col);
  if (term == NULL) return NULL;
  term->nat = cur(p)->nat;
  term->big = cur(p)->big;
  bump(p);
  return term;
}

static const Term *parse_sigma_tail(Parser *p) {
  if (!at_binder_group(p)) {
    fail_at(p, cur(p), "a binder (x : A) after 'Sigma'");
    return NULL;
  }
  Binders b;
  if (!parse_group(p, &b)) return NULL;
  const Term *body = parse_atom(p);
  return body == NULL ? NULL : wrap(p, TERM_SIGMA, &b, body);
}

/* `Sigma (x : A) Sigma (y : B) C` nests without parentheses, so it counts
   toward the depth limit. */
static const Term *parse_sigma(Parser *p) {
  bump(p);
  const Term *term = enter(p) ? parse_sigma_tail(p) : NULL;
  p->depth--;
  return term;
}

static const Term *parse_paren(Parser *p) {
  bump(p);
  const Term *inner = parse_term(p);
  return inner != NULL && expect(p, TOK_RPAREN, "')'") ? inner : NULL;
}

static const Term *parse_leaf(Parser *p, TermKind kind) {
  const Token *tok = cur(p);
  Term *term = make_term(p, kind, tok->line, tok->col);
  if (term == NULL) return NULL;
  term->nat = tok->nat;
  term->big = tok->big;
  term->name = token_text(p, tok);
  bump(p);
  return term->name == NULL ? NULL : term;
}

/* '~' atom: TERM_IVL with nat 2 (IVL_REV). */
static const Term *parse_rev(Parser *p) {
  const Token *tok = cur(p);
  bump(p);
  const Term *operand = enter(p) ? parse_atom(p) : NULL;
  p->depth--;
  Term *t = operand == NULL ? NULL : make_term(p, TERM_IVL, tok->line, tok->col);
  if (t == NULL) return NULL;
  t->nat = 2u;
  t->left = operand;
  return t;
}

/* One branch `face -> term` of a system. The face uses the interval grammar. */
static Term *parse_branch(Parser *p) {
  const Token *tok = cur(p);
  const Term *face = parse_join(p);
  if (face == NULL || !expect(p, TOK_ARROW, "'->'")) return NULL;
  const Term *body = parse_term(p);
  Term *t = body == NULL ? NULL : make_term(p, TERM_SYS, tok->line, tok->col);
  if (t == NULL) return NULL;
  t->left = face;
  t->right = body;
  return t;
}

/* '[' branch (',' branch)* ']': a system. Each branch links to the next. */
static const Term *parse_system(Parser *p) {
  bump(p);
  Term *head = parse_branch(p);
  Term *last = head;
  while (last != NULL && eat(p, TOK_COMMA)) {
    Term *next = parse_branch(p);
    last->next = next;
    last = next;
  }
  return last != NULL && expect(p, TOK_RBRACKET, "']' or ','") ? head : NULL;
}

static const Term *parse_atom(Parser *p) {
  const Token *tok = cur(p);
  switch (tok->kind) {
    case TOK_IDENT: return parse_leaf(p, TERM_VAR);
    case TOK_NAT: return parse_leaf(p, TERM_NAT);
    case TOK_LPAREN: return parse_paren(p);
    case TOK_KW_TYPE: return parse_universe(p);
    case TOK_KW_SIGMA: return parse_sigma(p);
    case TOK_TILDE: return parse_rev(p);
    case TOK_LBRACKET: return parse_system(p);
    case TOK_LANGLE:
    case TOK_RANGLE:
    case TOK_AT:
    case TOK_MEET:
    case TOK_JOIN:
    case TOK_RBRACKET:
    case TOK_COMMA:
    case TOK_EOF:
    case TOK_RPAREN:
    case TOK_COLON:
    case TOK_DEFINE:
    case TOK_ARROW:
    case TOK_FATARROW:
    case TOK_STAR:
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_EQEQ:
    case TOK_SEMI:
    case TOK_KW_CIRCUIT:
    case TOK_KW_PUBLIC:
    case TOK_KW_PRIVATE:
    case TOK_KW_HINT:
    case TOK_KW_RANGE8:
    case TOK_KW_MUTUAL:
    case TOK_BAR:
    case TOK_KW_DEF:
    case TOK_KW_REC:
    case TOK_KW_FUN:
    case TOK_KW_FAMILY:
    case TOK_KW_AXIOM:
      break;
  }
  fail_at(p, tok, "a term");
  return NULL;
}

/* Application and '@' share one left-associative level. */
static const Term *parse_app(Parser *p) {
  const Term *head = parse_atom(p);
  while (head != NULL && (at(p, TOK_AT) || starts_atom(cur(p)->kind))) {
    TermKind kind = eat(p, TOK_AT) ? TERM_PAPP : TERM_APP;
    const Term *arg = parse_atom(p);
    Term *app = arg == NULL ? NULL : make_term(p, kind, head->line, head->col);
    if (app == NULL) return NULL;
    app->left = head;
    app->right = arg;
    head = app;
  }
  return head;
}

/* TERM_ARITH: WHICH 0 +, 1 -, 2 *. The checker allows Field operands only.
   TERM_IVL: WHICH 3 /\, 4 \/. */
static const Term *binary(Parser *p, TermKind kind, const Term *left, uint64_t which, const Term *right) {
  Term *t = right == NULL ? NULL : make_term(p, kind, left->line, left->col);
  if (t == NULL) return NULL;
  t->nat = which;
  t->left = left;
  t->right = right;
  return t;
}

static const Term *parse_prod(Parser *p) {
  const Term *head = parse_app(p);
  while (head != NULL && eat(p, TOK_STAR)) head = binary(p, TERM_ARITH, head, 2u, parse_app(p));
  return head;
}

static const Term *parse_sum(Parser *p) {
  const Term *head = parse_prod(p);
  while (head != NULL && (at(p, TOK_PLUS) || at(p, TOK_MINUS))) {
    uint64_t which = at(p, TOK_PLUS) ? 0u : 1u;
    bump(p);
    head = binary(p, TERM_ARITH, head, which, parse_prod(p));
  }
  return head;
}

static const Term *parse_meet(Parser *p) {
  const Term *head = parse_sum(p);
  while (head != NULL && eat(p, TOK_MEET)) head = binary(p, TERM_IVL, head, 3u, parse_sum(p));
  return head;
}

static const Term *parse_join(Parser *p) {
  const Term *head = parse_meet(p);
  while (head != NULL && eat(p, TOK_JOIN)) head = binary(p, TERM_IVL, head, 4u, parse_meet(p));
  return head;
}

/* '<' NAME '>' term. */
static const Term *parse_plam(Parser *p) {
  const Token *tok = cur(p);
  bump(p);
  const char *name = take_name(p, "an interval name");
  if (name == NULL || !expect(p, TOK_RANGLE, "'>'")) return NULL;
  const Term *body = parse_term(p);
  Term *t = body == NULL ? NULL : make_term(p, TERM_PLAM, tok->line, tok->col);
  if (t == NULL) return NULL;
  t->name = name;
  t->right = body;
  return t;
}

static const Term *parse_term_body(Parser *p) {
  if (at(p, TOK_KW_FUN)) return parse_fun(p);
  if (at(p, TOK_LANGLE)) return parse_plam(p);
  if (at_binder_group(p)) return parse_binder_type(p);
  const Term *head = parse_join(p);
  if (head == NULL || !at(p, TOK_ARROW)) return head;
  const Token *arrow = cur(p);
  bump(p);
  const Term *body = parse_term(p);
  Term *pi = body == NULL ? NULL : make_term(p, TERM_PI, arrow->line, arrow->col);
  if (pi == NULL) return NULL;
  pi->name = NULL;
  pi->left = head;
  pi->right = body;
  return pi;
}

static const Term *parse_term(Parser *p) {
  const Term *term = enter(p) ? parse_term_body(p) : NULL;
  p->depth--;
  return term;
}

static int parse_fields(Parser *p, const Field **out, size_t *out_count) {
  Field *items = NULL;
  size_t count = 0;
  size_t cap = 0;
  while (at_binder_group(p)) {
    Binders b;
    if (!parse_group(p, &b)) return 0;
    for (size_t i = 0; i < b.count; i++) {
      items = grow(p, items, count, &cap, sizeof *items);
      if (items == NULL) return 0;
      items[count].name = b.names[i];
      items[count].type = b.type;
      items[count].line = b.tok->line;
      items[count].col = b.tok->col;
      count++;
    }
  }
  *out = items;
  *out_count = count;
  return 1;
}

static int parse_def(Parser *p, Decl *d) {
  bump(p);
  d->kind = DECL_DEF;
  d->is_rec = eat(p, TOK_KW_REC);
  d->name = take_name(p, "a definition name");
  if (d->name == NULL) return 0;
  p->def = d->name;
  if (!expect(p, TOK_COLON, "':'")) return 0;
  d->type = parse_term(p);
  if (d->type == NULL || !expect(p, TOK_DEFINE, "':='")) return 0;
  d->body = parse_term(p);
  return d->body != NULL;
}

static int parse_axiom(Parser *p, Decl *d) {
  bump(p);
  d->kind = DECL_AXIOM;
  d->name = take_name(p, "an axiom name");
  if (d->name == NULL) return 0;
  p->def = d->name;
  if (!expect(p, TOK_COLON, "':'")) return 0;
  d->type = parse_term(p);
  return d->type != NULL;
}

/* M4 s1: after '<', the interval binders NAME+ '>' and then the boundary
   system of a path constructor. */
static int parse_path_ctor(Parser *p, Ctor *ctor) {
  const char **names = NULL;
  size_t count = 0;
  size_t cap = 0;
  do {
    names = grow(p, names, count, &cap, sizeof *names);
    if (names == NULL) return 0;
    names[count] = take_name(p, count == 0 ? "an interval binder" : "an interval binder or '>'");
    if (names[count] == NULL) return 0;
    count++;
  } while (!eat(p, TOK_RANGLE));
  if (!at(p, TOK_LBRACKET)) return fail_at(p, cur(p), "'[' (the boundary system of a path constructor)");
  ctor->binders = names;
  ctor->binder_count = count;
  ctor->boundary = parse_system(p);
  return ctor->boundary != NULL;
}

static int parse_family(Parser *p, Decl *d) {
  bump(p);
  d->kind = DECL_FAMILY;
  d->name = take_name(p, "a family name");
  if (d->name == NULL) return 0;
  p->def = d->name;
  if (!parse_fields(p, &d->params, &d->param_count) || !expect(p, TOK_DEFINE, "':='")) return 0;
  Ctor *ctors = NULL;
  size_t count = 0;
  size_t cap = 0;
  do {
    ctors = grow(p, ctors, count, &cap, sizeof *ctors);
    if (ctors == NULL) return 0;
    Ctor *ctor = &ctors[count];
    count++;
    ctor->line = cur(p)->line;
    ctor->col = cur(p)->col;
    ctor->binders = NULL;
    ctor->binder_count = 0;
    ctor->boundary = NULL;
    ctor->name = take_name(p, "a constructor name");
    if (ctor->name == NULL || !parse_fields(p, &ctor->fields, &ctor->field_count)) return 0;
    if (eat(p, TOK_LANGLE)) {
      if (!parse_path_ctor(p, ctor)) return 0;
    } else if (at(p, TOK_LBRACKET)) {
      return fail_at(p, cur(p), "'|' or a declaration (a boundary system needs interval binders '<i>')");
    }
  } while (eat(p, TOK_BAR));
  d->ctors = ctors;
  d->ctor_count = count;
  return 1;
}

/* glue-lang: KW followed by binder groups, or nothing. */
static int parse_binders(Parser *p, TokKind kw, const Field **out, size_t *count) {
  *out = NULL;
  *count = 0;
  return eat(p, kw) ? parse_fields(p, out, count) : 1;
}

/* constraint := sum `==` sum | `range8` sum */
static int parse_constraint(Parser *p, Constraint *k) {
  k->line = cur(p)->line;
  k->col = cur(p)->col;
  k->rhs = NULL;
  if (eat(p, TOK_KW_RANGE8)) {
    k->kind = CONSTRAINT_RANGE8;
    k->lhs = parse_sum(p);
    return k->lhs != NULL;
  }
  k->kind = CONSTRAINT_EQ;
  k->lhs = parse_sum(p);
  if (k->lhs == NULL || !expect(p, TOK_EQEQ, "'=='")) return 0;
  k->rhs = parse_sum(p);
  return k->rhs != NULL;
}

/* M6 s5 (D14): (hint NAME := TERM)*. A hint term ends at the next `hint`
   or `:=`. The `type` of each Field holds the term. */
static int parse_hints(Parser *p, Decl *d) {
  Field *items = NULL;
  size_t count = 0;
  size_t cap = 0;
  while (at(p, TOK_KW_HINT)) {
    const Token *tok = cur(p);
    bump(p);
    items = grow(p, items, count, &cap, sizeof *items);
    if (items == NULL) return 0;
    items[count].line = tok->line;
    items[count].col = tok->col;
    items[count].name = take_name(p, "a hint name");
    if (items[count].name == NULL || !expect(p, TOK_DEFINE, "':='")) return 0;
    items[count].type = parse_term(p);
    if (items[count].type == NULL) return 0;
    count++;
  }
  d->hints = items;
  d->hint_count = count;
  return 1;
}

/* circuit NAME [public BINDERS] [private BINDERS] (hint NAME := TERM)* := constraint (; constraint)* */
static int parse_circuit(Parser *p, Decl *d) {
  bump(p);
  d->kind = DECL_CIRCUIT;
  d->name = take_name(p, "a circuit name");
  if (d->name == NULL) return 0;
  p->def = d->name;
  if (!parse_binders(p, TOK_KW_PUBLIC, &d->params, &d->param_count)) return 0;
  if (!parse_binders(p, TOK_KW_PRIVATE, &d->privates, &d->private_count)) return 0;
  if (!parse_hints(p, d)) return 0;
  if (!expect(p, TOK_DEFINE, "':='")) return 0;
  Constraint *items = NULL;
  size_t count = 0;
  size_t cap = 0;
  do {
    items = grow(p, items, count, &cap, sizeof *items);
    if (items == NULL || !parse_constraint(p, &items[count])) return 0;
    count++;
  } while (eat(p, TOK_SEMI));
  d->constraints = items;
  d->constraint_count = count;
  return 1;
}

/* M5 s4 (D6): `mutual` family family. parse_source reads the second family;
   a block of one family gets MUTUAL_BLOCK in the checker. */
static int parse_mutual(Parser *p, Decl *d) {
  bump(p);
  if (!at(p, TOK_KW_FAMILY))
    return fail_at(p, cur(p), "'family' (a mutual block holds two families)");
  d->block = 1;
  return parse_family(p, d);
}

static int parse_decl(Parser *p, Decl *d) {
  const Token *tok = cur(p);
  p->def = NULL;
  d->origin = p->origin;
  d->line = tok->line;
  d->col = tok->col;
  d->block = 0;
  d->hints = NULL;
  d->hint_count = 0;
  switch (tok->kind) {
    case TOK_KW_DEF: return parse_def(p, d);
    case TOK_KW_AXIOM: return parse_axiom(p, d);
    case TOK_KW_FAMILY: return parse_family(p, d);
    case TOK_KW_MUTUAL: return parse_mutual(p, d);
    case TOK_KW_CIRCUIT: return parse_circuit(p, d);
    case TOK_EOF:
    case TOK_IDENT:
    case TOK_NAT:
    case TOK_LPAREN:
    case TOK_RPAREN:
    case TOK_COLON:
    case TOK_DEFINE:
    case TOK_ARROW:
    case TOK_FATARROW:
    case TOK_STAR:
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_EQEQ:
    case TOK_SEMI:
    case TOK_KW_PUBLIC:
    case TOK_KW_PRIVATE:
    case TOK_KW_HINT:
    case TOK_KW_RANGE8:
    case TOK_BAR:
    case TOK_KW_REC:
    case TOK_KW_FUN:
    case TOK_KW_TYPE:
    case TOK_KW_SIGMA:
    case TOK_LANGLE:
    case TOK_RANGLE:
    case TOK_AT:
    case TOK_TILDE:
    case TOK_MEET:
    case TOK_JOIN:
    case TOK_LBRACKET:
    case TOK_RBRACKET:
    case TOK_COMMA:
      break;
  }
  return fail_at(p, tok, "'def', 'axiom', 'family', 'mutual' or 'circuit'");
}

int parse_source(Arena *arena, const char *source, Origin origin, const TokenList *toks, DeclList *list, Diag *diag) {
  Parser p = {
    .toks = toks->items, .count = toks->count, .pos = 0, .arena = arena, .diag = diag,
    .source = source, .def = NULL, .origin = origin, .depth = 0,
  };
  Decl *items = list->items;
  size_t count = list->count;
  size_t cap = list->cap;
  while (!at(&p, TOK_EOF)) {
    int block = at(&p, TOK_KW_MUTUAL);
    items = grow(&p, items, count, &cap, sizeof *items);
    if (items == NULL || !parse_decl(&p, &items[count])) return 0;
    count++;
    /* M5 s4: the second family of a mutual block. */
    if (block && at(&p, TOK_KW_FAMILY)) {
      items = grow(&p, items, count, &cap, sizeof *items);
      if (items == NULL || !parse_decl(&p, &items[count])) return 0;
      items[count].block = 2;
      count++;
    }
  }
  list->items = items;
  list->count = count;
  list->cap = cap;
  return 1;
}
