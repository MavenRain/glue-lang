/* The langc command line of the tcc-json kit (forked from tcc-wasm).
   Exit 0: ok. Exit 1: the program is refused. Exit 2: usage or IO. */
#include <stdio.h>
#include <string.h>

#include "circuit.h"
#include "front/check.h"
#include "front/front.h"
#include "json.h"

#define ARENA_LIMIT_BYTES ((size_t)1 << 30)

typedef enum {
  CMD_CHECK,
  CMD_EVAL,
  CMD_BUILD,
  CMD_CIRCUIT,
  CMD_WITNESS,
  CMD_SATCHECK
} Command;

typedef struct {
  Command command;
  const char *prog_path; /* satcheck: the circuit document */
  const char *witness_path; /* satcheck only */
  const char *entry;
  char **args;
  int arg_count;
  const char *out_path; /* build: NULL is stdout; circuit, witness: NULL is NAME.KIND.json */
} Options;

static const struct {
  const char *word;
  Command command;
} COMMANDS[] = {
  {"check", CMD_CHECK},
  {"eval", CMD_EVAL},
  {"build", CMD_BUILD},
  {"circuit", CMD_CIRCUIT},
  {"witness", CMD_WITNESS},
  {"satcheck", CMD_SATCHECK},
};

static int usage(FILE *err) {
  fputs("usage: langc check PROG\n"
        "       langc eval PROG NAME [ARGS...]\n"
        "       langc build PROG [-o OUT]\n"
        "       langc circuit PROG NAME [-o OUT]\n"
        "       langc witness PROG NAME BINDER=VALUE... [-o OUT]\n"
        "       langc satcheck CIRCUIT.json WITNESS.json\n", err);
  return 2;
}

static int find_command(const char *word, Command *out) {
  for (size_t i = 0; i < sizeof COMMANDS / sizeof COMMANDS[0]; i++) {
    if (strcmp(COMMANDS[i].word, word) == 0) {
      *out = COMMANDS[i].command;
      return 1;
    }
  }
  return 0;
}

static int parse_build_options(int argc, char **argv, Options *opt, Diag *diag) {
  if (argc == 3) return 1;
  if (argc == 5 && strcmp(argv[3], "-o") == 0) {
    opt->out_path = argv[4];
    return 1;
  }
  return diag_fail(diag, "USAGE", NULL, "build takes PROG and an optional -o OUT");
}

static int parse_options(int argc, char **argv, Options *opt, Diag *diag) {
  memset(opt, 0, sizeof *opt);
  if (argc < 3 || !find_command(argv[1], &opt->command)) {
    return diag_fail(diag, "USAGE", NULL, "expected a command and a program path");
  }
  opt->prog_path = argv[2];
  switch (opt->command) {
    case CMD_CHECK:
      return argc == 3 ? 1 : diag_fail(diag, "USAGE", NULL, "too many arguments");
    case CMD_EVAL:
      if (argc < 4) return diag_fail(diag, "USAGE", NULL, "eval needs a definition name");
      opt->entry = argv[3];
      opt->args = argv + 4;
      opt->arg_count = argc - 4;
      return 1;
    case CMD_BUILD:
      return parse_build_options(argc, argv, opt, diag);
    case CMD_CIRCUIT:
      if (argc < 4) return diag_fail(diag, "USAGE", NULL, "circuit needs a circuit name");
      opt->entry = argv[3];
      if (argc == 4) return 1;
      if (argc == 6 && strcmp(argv[4], "-o") == 0) {
        opt->out_path = argv[5];
        return 1;
      }
      return diag_fail(diag, "USAGE", NULL, "circuit takes PROG NAME and an optional -o OUT");
    case CMD_WITNESS: {
      int with_out = argc >= 6 && strcmp(argv[argc - 2], "-o") == 0;
      if (argc < 4) return diag_fail(diag, "USAGE", NULL, "witness needs a circuit name");
      opt->entry = argv[3];
      opt->args = argv + 4;
      opt->arg_count = argc - 4 - (with_out ? 2 : 0);
      opt->out_path = with_out ? argv[argc - 1] : NULL;
      return 1;
    }
    case CMD_SATCHECK:
      if (argc != 4) return diag_fail(diag, "USAGE", NULL, "satcheck takes CIRCUIT.json WITNESS.json");
      opt->witness_path = argv[3];
      return 1;
  }
  return 0;
}

static int read_source(Arena *arena, const char *path, char **out, size_t *len, Diag *diag) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) return diag_fail(diag, "IO", NULL, "cannot open %s", path);
  char *buf = arena_alloc(arena, SOURCE_MAX_BYTES + 1);
  size_t count = buf == NULL ? 0 : fread(buf, 1, SOURCE_MAX_BYTES + 1, file);
  int read_error = ferror(file);
  fclose(file);
  if (buf == NULL) return diag_fail(diag, "OOM", NULL, "no memory for %s", path);
  if (read_error) return diag_fail(diag, "IO", NULL, "cannot read %s", path);
  if (count > SOURCE_MAX_BYTES) return diag_fail(diag, "IO_SIZE", NULL, "%s is larger than %zu bytes", path, SOURCE_MAX_BYTES);
  *out = buf;
  *len = count;
  return 1;
}

/* Writes the document only after it is complete, so a failure leaves no file. */
static int write_output(const char *path, const char *text, size_t len, Diag *diag) {
  FILE *file = path == NULL ? stdout : fopen(path, "wb");
  if (file == NULL) return diag_fail(diag, "IO", NULL, "cannot open %s", path);
  size_t count = fwrite(text, 1, len, file);
  int closed = path == NULL ? fflush(file) : fclose(file);
  return count == len && closed == 0 ? 1 : diag_fail(diag, "IO", NULL, "cannot write %s", path == NULL ? "stdout" : path);
}

/* OUT, or NAME followed by SUFFIX in the current directory. */
static const char *out_or_default(Arena *arena, const char *out, const char *name, const char *suffix, Diag *diag) {
  if (out != NULL) return out;
  size_t name_len = strlen(name);
  size_t suffix_len = strlen(suffix);
  char *path = arena_alloc(arena, name_len + suffix_len + 1);
  if (path == NULL) {
    diag_fail(diag, "OOM", NULL, "no memory for the output path");
    return NULL;
  }
  memcpy(path, name, name_len);
  memcpy(path + name_len, suffix, suffix_len + 1);
  return path;
}

static int write_document(Arena *arena, const Options *opt, const char *suffix, const char *doc, size_t doc_len, Diag *diag) {
  const char *path = out_or_default(arena, opt->out_path, opt->entry, suffix, diag);
  return path != NULL && write_output(path, doc, doc_len, diag) ? 0 : 2;
}

/* satcheck reads two documents and no program. */
static int run_satcheck(const Options *opt, Arena *arena, Diag *diag) {
  char *circuit = NULL;
  char *witness = NULL;
  size_t circuit_len = 0;
  size_t witness_len = 0;
  if (!read_source(arena, opt->prog_path, &circuit, &circuit_len, diag)) return 2;
  if (!read_source(arena, opt->witness_path, &witness, &witness_len, diag)) return 2;
  if (!circuit_satcheck(arena, diag, circuit, circuit_len, witness, witness_len)) return 1;
  puts("ok");
  return 0;
}

static int run(const Options *opt, Arena *arena, Diag *diag) {
  char *text = NULL;
  size_t len = 0;
  if (opt->command == CMD_SATCHECK) return run_satcheck(opt, arena, diag);
  if (!read_source(arena, opt->prog_path, &text, &len, diag)) return 2;
  DeclList decls;
  if (!front_load(arena, opt->prog_path, text, len, &decls, diag)) return 1;
  Machine machine;
  if (!check_program(arena, &decls, &machine, diag)) return 1;
  switch (opt->command) {
    case CMD_CHECK:
      if (!circuit_check_all(&machine)) return 1;
      puts("ok");
      return 0;
    case CMD_EVAL:
      return eval_command(&machine, opt->entry, opt->args, opt->arg_count, stdout, stderr);
    case CMD_BUILD: {
      const char *doc = NULL;
      size_t doc_len = 0;
      if (!json_document(&machine, &doc, &doc_len)) return 1;
      return write_output(opt->out_path, doc, doc_len, diag) ? 0 : 2;
    }
    case CMD_CIRCUIT: {
      const char *doc = NULL;
      size_t doc_len = 0;
      if (!circuit_document(&machine, opt->entry, &doc, &doc_len)) return 1;
      return write_document(arena, opt, ".circuit.json", doc, doc_len, diag);
    }
    case CMD_WITNESS: {
      const char *doc = NULL;
      size_t doc_len = 0;
      int status = circuit_witness(&machine, opt->entry, opt->args, opt->arg_count, &doc, &doc_len);
      return status != 0 ? status : write_document(arena, opt, ".witness.json", doc, doc_len, diag);
    }
    case CMD_SATCHECK:
      return run_satcheck(opt, arena, diag);
  }
  return 1;
}

int main(int argc, char **argv) {
  Diag diag;
  diag_init(&diag);
  Options opt;
  if (!parse_options(argc, argv, &opt, &diag)) {
    diag_print(&diag, stderr);
    return usage(stderr);
  }
  Arena arena;
  arena_init(&arena, ARENA_LIMIT_BYTES);
  int status = run(&opt, &arena, &diag);
  arena_release(&arena);
  if (status != 0) diag_print(&diag, stderr);
  return status;
}
