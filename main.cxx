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
void dump_function_decl(tree fndecl);
void dump_identifier_node(tree id);
void dumpv_type(tree type);
void dumpv_decl(tree decl);
void dumpv_cst(tree cst);
void dumpv_expr(tree expr);
void dumpv_node(tree node);
void dumpv_unknown(tree node, int line);
std::string get_macro_info(source_location where, source_location* fix);

void dump_function_decl(tree fndecl) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(fndecl), tree_code_name[TREE_CODE(fndecl)]);
  location_t loc = DECL_SOURCE_LOCATION(fndecl);
  struct function* fn = DECL_STRUCT_FUNCTION(fndecl);
  location_t start = fn->function_start_locus, end = fn->function_end_locus;
  fprintf(os, "%s:%d:%d %s:%d:%d\n", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc), LOCATION_FILE(end), LOCATION_LINE(end), LOCATION_COLUMN(end));
  indent.add();
  dump_identifier_node(DECL_NAME(fndecl));
  dumpv_decl(DECL_RESULT(fndecl));
  for (tree args = DECL_ARGUMENTS(fndecl); args; args = DECL_CHAIN(args))
    dumpv_decl(args);
  dumpv_expr(DECL_SAVED_TREE(fndecl));
  indent.del();
}
void dump_identifier_node(tree id) {
  if (id == NULL_TREE)
    return;
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(id), tree_code_name[TREE_CODE(id)]);
  fprintf(os, "%s\n", IDENTIFIER_POINTER(id));
}
void dumpv_decl(tree decl) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(decl), tree_code_name[TREE_CODE(decl)]);
  location_t loc = DECL_SOURCE_LOCATION(decl);
  fprintf(os, "%s:%d:%d\n", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dump_identifier_node(DECL_NAME(decl));
  dumpv_type(TREE_TYPE(decl));
  indent.del();
}
void dumpv_expr(tree expr) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(expr), tree_code_name[TREE_CODE(expr)]);
  location_t tmp = EXPR_LOCATION(expr), loc;
  std::string macro_info = get_macro_info(tmp, &loc);
  fprintf(os, "%s:%d:%d ", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  fprintf(os, "(%s)\n", macro_info.c_str());
  indent.add();
  switch (TREE_CODE(expr)) {
  case BIND_EXPR:
    for (tree vardecl = BIND_EXPR_VARS(expr); vardecl; vardecl = TREE_CHAIN(vardecl)) {
    }
    dumpv_node(BIND_EXPR_BODY(expr));
    break;
  case VA_ARG_EXPR:
    dumpv_node(TREE_OPERAND(expr, 0));
    dumpv_node(TREE_TYPE(expr));
    break;
  case CALL_EXPR: {
    dumpv_node(CALL_EXPR_FN(expr));
    int len = call_expr_nargs(expr);
    for (int i = 0; i < len; ++i)
      dumpv_node(CALL_EXPR_ARG(expr, i));
    break;
  }
  case ASM_EXPR: {
    dumpv_cst(ASM_STRING(expr));
    fprintf(os, "%sNA asm_outputs\n", indent.c_str());
    indent.add();
    for (tree it = ASM_OUTPUTS(expr); it; it = TREE_CHAIN(it)) {
      fprintf(os, "%sNA asm_operand\n", indent.c_str());
      indent.add();
      dumpv_node(TREE_PURPOSE(TREE_PURPOSE(it)));  // can be NULL, or we can use `dumpv_cst`
      dumpv_cst(TREE_VALUE(TREE_PURPOSE(it)));
      dumpv_node(TREE_VALUE(it));
      indent.del();
    }
    indent.del();
    fprintf(os, "%sNA asm_inputs\n", indent.c_str());
    indent.add();
    for (tree it = ASM_INPUTS(expr); it; it = TREE_CHAIN(it)) {
      fprintf(os, "%sNA asm_operand\n", indent.c_str());
      indent.add();
      dumpv_node(TREE_PURPOSE(TREE_PURPOSE(it)));  // can be NULL, or we can use `dumpv_cst`
      dumpv_cst(TREE_VALUE(TREE_PURPOSE(it)));
      dumpv_node(TREE_VALUE(it));
      indent.del();
    }
    indent.del();
    fprintf(os, "%sNA asm_clobbers\n", indent.c_str());
    indent.add();
    for (tree it = ASM_CLOBBERS(expr); it; it = TREE_CHAIN(it)) {
      assert(TREE_PURPOSE(it) == NULL_TREE);
      dumpv_cst(TREE_VALUE(it));
    }
    indent.del();
    fprintf(os, "%sNA asm_labels\n", indent.c_str());
    indent.add();
    for (tree it = ASM_LABELS(expr); it; it = TREE_CHAIN(it)) {
      dumpv_cst(TREE_PURPOSE(it));
      // dump_identifier_node(DECL_NAME(TREE_VALUE(it)));
    }
    indent.del();
    break;
  }
  default: {
    int len = TREE_OPERAND_LENGTH(expr);
    for (int i = 0; i < len; ++i) {
      bool skip = false;
      skip = skip || (TREE_CODE(expr) == ARRAY_REF && (i == 2 || i == 3));
      skip = skip || (TREE_CODE(expr) == COMPONENT_REF && (i == 2));
      skip = skip || (TREE_CODE(expr) == TARGET_EXPR && (i == 2 || i == 3));
      skip = skip || (TREE_CODE(expr) == CASE_LABEL_EXPR && (i == 3));
      if (skip)
        continue;
      dumpv_node(TREE_OPERAND(expr, i));
    }
  }
  }
  indent.del();
}
void dumpv_type(tree type) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(type), tree_code_name[TREE_CODE(type)]);
  tree typedecl = TYPE_NAME(type);
  if (typedecl != NULL_TREE) {  // boolean, integer, void, record, union
    if (TREE_CODE(typedecl) == IDENTIFIER_NODE) {  // FIXME https://gcc.gnu.org/onlinedocs/gcc-4.8.4/gccint/Types.html#index-TYPE_005fNAME-2377
      fprintf(os, "structBUG %s\n", IDENTIFIER_POINTER(typedecl));
      return;
    }
    fprintf(os, "%s\n", IDENTIFIER_POINTER(DECL_NAME(typedecl)));
  } else if (TREE_CODE(type) == POINTER_TYPE || TREE_CODE(type) == ARRAY_TYPE) {
    fprintf(os, "%s\n", (TREE_CODE(type) == POINTER_TYPE) ? "*" : "[]");
    indent.add();
    dumpv_type(TREE_TYPE(type));
    indent.del();
  } else {  // enumeral
    fprintf(os, "<unknown>\n");
  }
}
void dumpv_node(tree node) {
  if (!node) {
    fprintf(os, "%sNULL_xx\n", indent.c_str());
    return;
  }
  if (TREE_CODE(node) == STATEMENT_LIST) {
    fprintf(os, "%s%d %s\n", indent.c_str(), TREE_CODE(node), tree_code_name[TREE_CODE(node)]);
    indent.add();
    for (tree_stmt_iterator it = tsi_start(node); !tsi_end_p(it); tsi_next(&it))
      dumpv_node(tsi_stmt(it));
    indent.del();
  } else if (EXPR_P(node)) {
    dumpv_expr(node);
  } else if (CONSTANT_CLASS_P(node)) {
    dumpv_cst(node);
  } else if (DECL_P(node)) {
    dumpv_decl(node);
  } else if (TYPE_P(node)) {
    dumpv_type(node);
  } else if (TREE_CODE(node) == TREE_LIST) {
    fprintf(os, "%s%d %s TODO_%d\n", indent.c_str(), TREE_CODE(node), tree_code_name[TREE_CODE(node)], __LINE__);
    indent.add();
    /*
    for (tree it = node; it; it = TREE_CHAIN(it)) {
      dumpv_unknown(TREE_VALUE(it), __LINE__);
      dumpv_unknown(TREE_PURPOSE(it), __LINE__);
    }
    */
    dumpv_node(TREE_VALUE(node));
    dumpv_node(TREE_PURPOSE(node));
    dumpv_node(TREE_CHAIN(node));
    indent.del();
  } else {
    dumpv_unknown(node, __LINE__);
  }
}
#define TREE_INT_CST_VALUE(e) ((TREE_INT_CST_HIGH (e) << HOST_BITS_PER_WIDE_INT) + TREE_INT_CST_LOW (e))
void dumpv_cst(tree cst) {
  fprintf(os, "%s%d %s ", indent.c_str(), TREE_CODE(cst), tree_code_name[TREE_CODE(cst)]);
  switch (TREE_CODE(cst)) {
  case INTEGER_CST:
    fprintf(os, "%d\n", TREE_INT_CST_VALUE(cst));
    break;
  case STRING_CST: {
    std::string tmp(TREE_STRING_POINTER(cst), TREE_STRING_LENGTH(cst));
    for (std::string::iterator it = tmp.begin(); it != tmp.end(); ++it)
      if (*it == '\n' || *it == '\0')
        *it = ' ';
    fprintf(os, "%s\n", tmp.c_str());
    break;
  }
  default:
    fprintf(os, "<cst>\n");
  }
}
void dumpv_unknown(tree node, int line) {
  if (!node) {
    fprintf(os, "%sNULL TODO_%d\n", indent.c_str(), line);
    return;
  }
  fprintf(os, "%s%d %s TODO_%d\n", indent.c_str(), TREE_CODE(node), tree_code_name[TREE_CODE(node)], line);
}

typedef struct {
  const struct line_map *map;
  source_location where;
} loc_map_pair;
std::string get_macro_info(source_location where, source_location* fix) {
  if (fix != NULL)
    *fix = where;
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
  bool start = false;
  source_location last;
  FOR_EACH_VEC_ELT(loc_vec, ix, iter) {
    if (!start) {
      source_location resolved_def_loc = linemap_resolve_location (line_table, iter->where, LRK_MACRO_DEFINITION_LOCATION, NULL);
      source_location resolved_spl_loc = linemap_resolve_location (line_table, iter->where, LRK_SPELLING_LOCATION, NULL);
      if (linemap_compare_locations(line_table, resolved_def_loc, resolved_spl_loc) == 0) {
        start = true;
        res << LOCATION_FILE(resolved_def_loc) << ":" << LOCATION_LINE(resolved_def_loc) << ":" << LOCATION_COLUMN(resolved_def_loc);
      }
      last = resolved_spl_loc;
    }
    if (start) {  // should NOT be `else` clause of the above `if`
      res << " " << linemap_map_get_macro_name(iter->map) << " ";
      source_location resolved_exp_loc = linemap_resolve_location (line_table, MACRO_MAP_EXPANSION_POINT_LOCATION (iter->map), LRK_MACRO_DEFINITION_LOCATION, NULL);
      res << LOCATION_FILE(resolved_exp_loc) << ":" << LOCATION_LINE(resolved_exp_loc) << ":" << LOCATION_COLUMN(resolved_exp_loc);
      last = resolved_exp_loc;
    }
  }
  if (fix != NULL)
    *fix = last;
  loc_vec.release ();
  return res.str();
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

void callback_pre_genericize (void *gcc_data, void *user_data)
{
  tree fndecl = (tree)gcc_data;
  dump_function_decl(fndecl);
}
void callback_type (void *gcc_data, void *user_data)
{
  // Only called with `struct` or `union`, not even `enum`
  tree spec = (tree)gcc_data;
  // INSPECT_TREE_CODE(spec);
}
void callback_decl (void *gcc_data, void *user_data)
{
  tree decl = (tree)gcc_data;
  if (TREE_CODE(decl) == PARM_DECL || current_function_decl != NULL)
    return;
  if (TREE_CODE(decl) == FUNCTION_DECL) {
    tree fndecl = (tree)gcc_data;
    // dump_function_decl_incomplete(fndecl);
    return;
  } else if (TREE_CODE(decl) == VAR_DECL) {
    tree vardecl = (tree)gcc_data;
    dumpv_decl(vardecl);
    return;
  }
  INSPECT_TREE_CODE(decl);
}

