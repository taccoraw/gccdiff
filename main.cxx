// vim:et ts=2 sts=2 sw=2
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-iterator.h"
#include "function.h"
#include <cstdio>
#include <string>
#include <sstream>
#include <cassert>

#define INSPECT_TREE_CODE(tree) fprintf(os, "TREE_CODE(" #tree ") = %s\n", tree_code_name[TREE_CODE(tree)])
#define SHOW_LOC(decl) fprintf(os, "%s:%d:%d\n", DECL_SOURCE_FILE(decl), DECL_SOURCE_LINE(decl), DECL_SOURCE_COLUMN(decl))
#define PRINT_LOC(loc) fprintf(os, "%s:%d:%d\n", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc))

int plugin_is_GPL_compatible;

static void callback_pre_genericize (void *gcc_data, void *user_data);
static void callback_type (void *gcc_data, void *user_data);
static void callback_decl (void *gcc_data, void *user_data);
static void callback_finish (void *gcc_data, void *user_data);

static FILE* os = NULL;
static bool allow_empty = false;

int plugin_init (struct plugin_name_args *plugin_info,
                 struct plugin_gcc_version *version)
{
  if (!plugin_default_version_check(version, &gcc_version))
    return 1;
  register_callback(plugin_info->base_name, PLUGIN_PRE_GENERICIZE, callback_pre_genericize, NULL);
  // register_callback(plugin_info->base_name, PLUGIN_FINISH_TYPE, callback_type, NULL);
  // register_callback(plugin_info->base_name, PLUGIN_FINISH_DECL, callback_decl, NULL);
  register_callback(plugin_info->base_name, PLUGIN_FINISH, callback_finish, NULL);
  std::ostringstream oss;
  oss << main_input_filename << ".ind";
  os = fopen(oss.str().c_str(), "w");
  return 0;
}

void callback_finish (void *gcc_data, void *user_data)
{
  fclose(os);
  os = NULL;
}

class Indent {
  std::string _indent, _delta;
public:
  Indent() : _delta("  ") {}
  void add() {
    _indent += _delta;
  }
  void del() {
    assert(_indent.length() >= _delta.length());
    _indent.resize(_indent.length() - _delta.length());
  }
  const char* c_str() {
    return _indent.c_str();
  }
} indent;
void dumpv_unknown(tree node);
void dump_function_decl(tree fndecl);
void dump_result_decl(tree resdecl);
void dump_parm_decl(tree args);
void dumpv_type(tree type);
void dump_function_decl_incomplete(tree fndecl);
void dump_function_type(tree fntype);
void dump_var_decl(tree vardecl);
void dump_bind_expr(tree block);
void dumpv_body(tree body);
void dump_statement_list(tree stmts);
void dumpv_block_item(tree item);
void dumpv_expr(tree expr);
void dumpv_constant(tree constant);
void dumpv_decl_ref(tree decl);

typedef struct {
  const struct line_map *map;
  source_location where;
} loc_map_pair;
std::string get_macro_info(source_location where) {
  const struct line_map* map = linemap_lookup(line_table, where);
  if (!linemap_macro_expansion_map_p(map))
    return std::string("");

  loc_map_pair loc;
  vec<loc_map_pair> loc_vec = vNULL;
  do {
    loc.where = where;
    loc.map = map;
    loc_vec.safe_push(loc);
    where = linemap_unwind_toward_expansion(line_table, where, &map);
  } while (linemap_macro_expansion_map_p(map));

  std::ostringstream res;
  unsigned ix;
  loc_map_pair* iter;
  FOR_EACH_VEC_ELT(loc_vec, ix, iter) {
    if (ix == 0) {
      source_location resolved_def_loc = linemap_resolve_location (line_table, iter->where, LRK_MACRO_DEFINITION_LOCATION, NULL);
      res << LOCATION_FILE(resolved_def_loc) << ":" << LOCATION_LINE(resolved_def_loc) << ":" << LOCATION_COLUMN(resolved_def_loc);
    }
    res << " " << linemap_map_get_macro_name(iter->map) << " ";
    source_location resolved_exp_loc = linemap_resolve_location (line_table, MACRO_MAP_EXPANSION_POINT_LOCATION (iter->map), LRK_MACRO_DEFINITION_LOCATION, NULL);
    res << LOCATION_FILE(resolved_exp_loc) << ":" << LOCATION_LINE(resolved_exp_loc) << ":" << LOCATION_COLUMN(resolved_exp_loc);
  }
  loc_vec.release ();
  return res.str();
}

std::string _get_type_name(tree type) {
  tree typedecl = TYPE_NAME(type);
  if (typedecl != NULL_TREE) {
    if (TREE_CODE(typedecl) == IDENTIFIER_NODE) {  // FIXME https://gcc.gnu.org/onlinedocs/gcc-4.8.4/gccint/Types.html#index-TYPE_005fNAME-2377
      return std::string("struct ") + IDENTIFIER_POINTER(typedecl);
    }
    return IDENTIFIER_POINTER(DECL_NAME(typedecl));
  } else if (TREE_CODE(type) == POINTER_TYPE) {
    return _get_type_name(TREE_TYPE(type)) + "*";
  } else
    return "<unknown> TDO_type";
}
const char* get_type_name(tree type) {
  static std::string s;  // not re-entryable!
  s = _get_type_name(type);
  return s.c_str();
}

void dump_unknown(tree node) {
  if (!node) {
    fprintf(os, "%s0x%08x TODO_%d\n", indent.c_str(), node, __LINE__);
    return;
  }
  fprintf(os, "%s%d %s ??? TODO_%d\n", indent.c_str(), TREE_CODE(node), tree_code_name[TREE_CODE(node)], __LINE__);
}
void dump_unknown_tree_list(tree node) {
  /*if (!node) {
    fprintf(os, "%s0x%08x TODO_%d\n", indent.c_str(), node, __LINE__);
    return;
  }
  fprintf(os, "%s%d %s () TODO_%d\n", indent.c_str(), TREE_CODE(node), tree_code_name[TREE_CODE(node)], __LINE__);
  indent.add();
  for (tree it = node; it; it = TREE_CHAIN(it)) {
    dump_unknown(TREE_VALUE(it));
  }
  indent.del();*/
}
void dump_function_decl(tree fndecl) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(fndecl), tree_code_name[TREE_CODE(fndecl)]);
  const char* label = (DECL_NAME(fndecl) ? IDENTIFIER_POINTER(DECL_NAME(fndecl)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(fndecl);
  struct function* fn = DECL_STRUCT_FUNCTION(fndecl);
  location_t start = fn->function_start_locus, end = fn->function_end_locus;
  fprintf(os, "%s %s:%d:%d %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc), LOCATION_FILE(end), LOCATION_LINE(end), LOCATION_COLUMN(end));
  indent.add();
  dump_result_decl(DECL_RESULT(fndecl));
  for (tree args = DECL_ARGUMENTS(fndecl); args; args = DECL_CHAIN(args))
    dump_parm_decl(args);
  dump_bind_expr(DECL_SAVED_TREE(fndecl));
  indent.del();
}
void dump_result_decl(tree resdecl) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(resdecl), tree_code_name[TREE_CODE(resdecl)]);
  const char* label = (DECL_NAME(resdecl) ? IDENTIFIER_POINTER(DECL_NAME(resdecl)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(resdecl);
  fprintf(os, "%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dumpv_type(TREE_TYPE(resdecl));
  indent.del();
}
void dump_parm_decl(tree args) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(args), tree_code_name[TREE_CODE(args)]);
  const char* label = (DECL_NAME(args) ? IDENTIFIER_POINTER(DECL_NAME(args)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(args);
  fprintf(os, "%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dumpv_type(TREE_TYPE(args));
  indent.del();
}
void dumpv_type(tree type) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(type), tree_code_name[TREE_CODE(type)]);
  fprintf(os, "%s\n", get_type_name(type));
}
void dump_function_decl_incomplete(tree fndecl) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(fndecl), tree_code_name[TREE_CODE(fndecl)]);
  const char* label = (DECL_NAME(fndecl) ? IDENTIFIER_POINTER(DECL_NAME(fndecl)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(fndecl);
  fprintf(os, "%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dump_function_type(TREE_TYPE(fndecl));
  indent.del();
}
void dump_function_type(tree fntype) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(fntype), tree_code_name[TREE_CODE(fntype)]);
  fprintf(os, "()\n");
  indent.add();
  dumpv_type(TREE_TYPE(fntype));
  indent.del();
}
void dump_var_decl(tree vardecl) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(vardecl), tree_code_name[TREE_CODE(vardecl)]);
  const char* label = (DECL_NAME(vardecl) ? IDENTIFIER_POINTER(DECL_NAME(vardecl)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(vardecl);
  fprintf(os, "%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dumpv_type(TREE_TYPE(vardecl));
  indent.del();
}
void dump_bind_expr(tree block) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(block), tree_code_name[TREE_CODE(block)]);
  location_t loc = EXPR_LOCATION(block);
  fprintf(os, "() %s:%d:%d\n", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  for (tree vardecl = BIND_EXPR_VARS(block); vardecl; vardecl = TREE_CHAIN(vardecl)) {
    if (TREE_CODE(vardecl) == VAR_DECL)
      dump_var_decl(vardecl);
    else
      fprintf(os, "%s%d %s ??? TDO_%d\n", indent.c_str(), TREE_CODE(vardecl), tree_code_name[TREE_CODE(vardecl)], __LINE__);
  }
  dumpv_body(BIND_EXPR_BODY(block));
  // dump_unknown(BIND_EXPR_BLOCK(block));
  indent.del();
}
void dumpv_body(tree body) {
  switch (TREE_CODE(body)) {
  case STATEMENT_LIST:
    dump_statement_list(body);
    break;
  // GUESS: when only one statement, no statement list here
  default:
    dumpv_block_item(body);
  }
}
void dump_statement_list(tree stmts) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(stmts), tree_code_name[TREE_CODE(stmts)]);
  fprintf(os, "()\n");
  indent.add();
  for (tree_stmt_iterator it = tsi_start(stmts); !tsi_end_p(it); tsi_next(&it))
    dumpv_block_item(tsi_stmt(it));
  indent.del();
}
void dumpv_block_item(tree item) {
  if (!item) {
    if (allow_empty)
      fprintf(os, "%sNULL_xx\n", indent.c_str());
    else
      fprintf(os, "%s0x%08x TODO_%d\n", indent.c_str(), item, __LINE__);
    allow_empty = false;
    return;
  }
  if (TREE_CODE(item) == BIND_EXPR) {
    dump_bind_expr(item);
    return;
  }
  if (EXPR_P(item)) {
    dumpv_expr(item);
    return;
  }
  switch (TREE_CODE(item)) {
  case INTEGER_CST:
  case STRING_CST:
    dumpv_constant(item);
    break;
  case RESULT_DECL:
  case FUNCTION_DECL:
  case VAR_DECL:
  case PARM_DECL:
  case FIELD_DECL:  // TODO should the following be merged or separated?
  case LABEL_DECL:
    dumpv_decl_ref(item);
    break;
  case STATEMENT_LIST:
    dump_statement_list(item);
    break;
  case TREE_LIST:
    dump_unknown_tree_list(item);
    break;
  default:
    dump_unknown(item);
  }
}
void dumpv_expr(tree expr) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(expr), tree_code_name[TREE_CODE(expr)]);
  location_t loc = EXPR_LOCATION(expr);
  fprintf(os, "() %s:%d:%d (%s)\n", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc), get_macro_info(loc).c_str());
  if (TREE_CODE(expr) == CALL_EXPR) {
    indent.add();
    dumpv_block_item(CALL_EXPR_FN(expr));
    int len = call_expr_nargs(expr);
    for (int i = 0; i < len; ++i)
      dumpv_block_item(CALL_EXPR_ARG(expr, i));
    indent.del();
    return;
  }
  indent.add();
  int len = TREE_OPERAND_LENGTH(expr);
  for (int i = 0; i < len; ++i) {
    bool skip = false;
    skip = skip || (TREE_CODE(expr) == COMPONENT_REF && i == 2);
    skip = skip || (TREE_CODE(expr) == CASE_LABEL_EXPR && i == 3);
    skip = skip || (TREE_CODE(expr) == ARRAY_REF && (i == 2 || i == 3));
    skip = skip || (TREE_CODE(expr) == TARGET_EXPR && (i == 2) || (i == 3));
    if (skip)
      continue;
    allow_empty = allow_empty || (TREE_CODE(expr) == RETURN_EXPR && i == 0 && TREE_OPERAND(expr, i) == NULL_TREE);
    allow_empty = allow_empty || (TREE_CODE(expr) == COND_EXPR && i == 2 && TREE_OPERAND(expr, i) == NULL_TREE);
    allow_empty = allow_empty || (TREE_CODE(expr) == ASM_EXPR && i >= 1 && TREE_OPERAND(expr, i) == NULL_TREE);
    allow_empty = allow_empty || (TREE_CODE(expr) == CASE_LABEL_EXPR && (i == 0 || i == 1 ));
    allow_empty = allow_empty || (TREE_CODE(expr) == SWITCH_EXPR && (i == 2));
    dumpv_block_item(TREE_OPERAND(expr, i));
  }
  indent.del();
}
#define TREE_INT_CST_VALUE(e) ((TREE_INT_CST_HIGH (e) << HOST_BITS_PER_WIDE_INT) + TREE_INT_CST_LOW (e))
void dumpv_constant(tree constant) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(constant), tree_code_name[TREE_CODE(constant)]);
  if (TREE_CODE(constant) == INTEGER_CST) {
    fprintf(os, "%d\n", TREE_INT_CST_VALUE(constant));
    return;
  }
  if (TREE_CODE(constant) == STRING_CST) {
    std::string tmp = TREE_STRING_POINTER(constant);
    for (std::string::iterator it = tmp.begin(); it != tmp.end(); ++it)
      if (*it == '\n')
        *it = ' ';
    fprintf(os, "%s\n", tmp.c_str());
    return;
  }
  fprintf(os, "(TODO_%d)\n", __LINE__);
}
void dumpv_decl_ref(tree decl) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(decl), tree_code_name[TREE_CODE(decl)]);
  const char* label = (DECL_NAME(decl) ? IDENTIFIER_POINTER(DECL_NAME(decl)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(decl);
  fprintf(os, "%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
}

void callback_pre_genericize (void *gcc_data, void *user_data)
{
  tree fndecl = (tree)gcc_data;
  dump_function_decl(fndecl);
}
void callback_type (void *gcc_data, void *user_data)
{
  // Only called with `struct` or `union`, not even `enum`
  tree spec = (tree)gcc_data;
  INSPECT_TREE_CODE(spec);
}
void callback_decl (void *gcc_data, void *user_data)
{
  tree decl = (tree)gcc_data;
  if (TREE_CODE(decl) == PARM_DECL || current_function_decl != NULL)
    return;
  if (TREE_CODE(decl) == FUNCTION_DECL) {
    tree fndecl = (tree)gcc_data;
    dump_function_decl_incomplete(fndecl);
    return;
  } else if (TREE_CODE(decl) == VAR_DECL) {
    tree vardecl = (tree)gcc_data;
    dump_var_decl(vardecl);
    return;
  }
  INSPECT_TREE_CODE(decl);
}

