// vim:et ts=2 sts=2 sw=2
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "tree-iterator.h"
#include "function.h"
#include <cstdio>
#include <string>
#include <cassert>

#define INSPECT_TREE_CODE(tree) printf("TREE_CODE(" #tree ") = %s\n", tree_code_name[TREE_CODE(tree)])
#define SHOW_LOC(decl) printf("%s:%d:%d\n", DECL_SOURCE_FILE(decl), DECL_SOURCE_LINE(decl), DECL_SOURCE_COLUMN(decl))
#define PRINT_LOC(loc) printf("%s:%d:%d\n", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc))

int plugin_is_GPL_compatible;

static void callback_pre_genericize (void *gcc_data, void *user_data);
static void callback_type (void *gcc_data, void *user_data);
static void callback_decl (void *gcc_data, void *user_data);

int plugin_init (struct plugin_name_args *plugin_info,
                 struct plugin_gcc_version *version)
{
  if (!plugin_default_version_check(version, &gcc_version))
    return 1;
  register_callback(plugin_info->base_name, PLUGIN_PRE_GENERICIZE, callback_pre_genericize, NULL);
  register_callback(plugin_info->base_name, PLUGIN_FINISH_TYPE, callback_type, NULL);
  register_callback(plugin_info->base_name, PLUGIN_FINISH_DECL, callback_decl, NULL);
  return 0;
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

std::string _get_type_name(tree type) {
  tree typedecl = TYPE_NAME(type);
  if (typedecl != NULL_TREE)
    return IDENTIFIER_POINTER(DECL_NAME(typedecl));
  else if (TREE_CODE(type) == POINTER_TYPE)
    return _get_type_name(TREE_TYPE(type)) + "*";
  else
    return "<unknown>";
}
const char* get_type_name(tree type) {
  static std::string s;  // not re-entryable!
  s = _get_type_name(type);
  return s.c_str();
}

void dump_unknown(tree node) {
  if (!node) {
    printf("%s0x%08x\n", indent.c_str(), node);
    return;
  }
  printf("%s%d %s ???\n", indent.c_str(), TREE_CODE(node), tree_code_name[TREE_CODE(node)]);
}
void dump_function_decl(tree fndecl) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(fndecl), tree_code_name[TREE_CODE(fndecl)]);
  const char* label = (DECL_NAME(fndecl) ? IDENTIFIER_POINTER(DECL_NAME(fndecl)) : "(unnamed)");
  /*
  struct function* fn = DECL_STRUCT_FUNCTION(fndecl);
  location_t start = fn->function_start_locus, end = fn->function_end_locus;
  printf("%s %s:%d:%d %s:%d:%d\n", label, LOCATION_FILE(start), LOCATION_LINE(start), LOCATION_COLUMN(start), LOCATION_FILE(end), LOCATION_LINE(end), LOCATION_COLUMN(end));
  */
  location_t loc = DECL_SOURCE_LOCATION(fndecl);
  printf("%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dump_result_decl(DECL_RESULT(fndecl));
  for (tree args = DECL_ARGUMENTS(fndecl); args; args = DECL_CHAIN(args))
    dump_parm_decl(args);
  dump_bind_expr(DECL_SAVED_TREE(fndecl));
  indent.del();
}
void dump_result_decl(tree resdecl) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(resdecl), tree_code_name[TREE_CODE(resdecl)]);
  const char* label = (DECL_NAME(resdecl) ? IDENTIFIER_POINTER(DECL_NAME(resdecl)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(resdecl);
  printf("%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dumpv_type(TREE_TYPE(resdecl));
  indent.del();
}
void dump_parm_decl(tree args) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(args), tree_code_name[TREE_CODE(args)]);
  const char* label = (DECL_NAME(args) ? IDENTIFIER_POINTER(DECL_NAME(args)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(args);
  printf("%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dumpv_type(TREE_TYPE(args));
  indent.del();
}
void dumpv_type(tree type) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(type), tree_code_name[TREE_CODE(type)]);
  printf("%s\n", get_type_name(type));
}
void dump_function_decl_incomplete(tree fndecl) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(fndecl), tree_code_name[TREE_CODE(fndecl)]);
  const char* label = (DECL_NAME(fndecl) ? IDENTIFIER_POINTER(DECL_NAME(fndecl)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(fndecl);
  printf("%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dump_function_type(TREE_TYPE(fndecl));
  indent.del();
}
void dump_function_type(tree fntype) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(fntype), tree_code_name[TREE_CODE(fntype)]);
  printf("()\n");
  indent.add();
  dumpv_type(TREE_TYPE(fntype));
  indent.del();
}
void dump_var_decl(tree vardecl) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(vardecl), tree_code_name[TREE_CODE(vardecl)]);
  const char* label = (DECL_NAME(vardecl) ? IDENTIFIER_POINTER(DECL_NAME(vardecl)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(vardecl);
  printf("%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  dumpv_type(TREE_TYPE(vardecl));
  indent.del();
}
void dump_bind_expr(tree block) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(block), tree_code_name[TREE_CODE(block)]);
  location_t loc = EXPR_LOCATION(block);
  printf("() %s:%d:%d\n", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
  indent.add();
  for (tree vardecl = BIND_EXPR_VARS(block); vardecl; vardecl = TREE_CHAIN(vardecl))
    dump_var_decl(vardecl);
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
  printf("%s%d %s ", indent.c_str(), TREE_CODE(stmts), tree_code_name[TREE_CODE(stmts)]);
  printf("()\n");
  indent.add();
  for (tree_stmt_iterator it = tsi_start(stmts); !tsi_end_p(it); tsi_next(&it))
    dumpv_block_item(tsi_stmt(it));
  indent.del();
}
void dumpv_block_item(tree item) {
  if (!item) {
    printf("%s0x%08x\n", indent.c_str(), item);
    return;
  }
  if (EXPR_P(item)) {
    dumpv_expr(item);
    return;
  }
  switch (TREE_CODE(item)) {
  case INTEGER_CST:
    dumpv_constant(item);
    break;
  case RESULT_DECL:
  case FUNCTION_DECL:
  case VAR_DECL:
  case PARM_DECL:
    dumpv_decl_ref(item);
    break;
  default:
    dump_unknown(item);
  }
}
void dumpv_expr(tree expr) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(expr), tree_code_name[TREE_CODE(expr)]);
  location_t loc = EXPR_LOCATION(expr);
  printf("() %s:%d:%d\n", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
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
  for (int i = 0; i < len; ++i)
    dumpv_block_item(TREE_OPERAND(expr, i));
  indent.del();
}
#define TREE_INT_CST_VALUE(e) ((TREE_INT_CST_HIGH (e) << HOST_BITS_PER_WIDE_INT) + TREE_INT_CST_LOW (e))
void dumpv_constant(tree constant) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(constant), tree_code_name[TREE_CODE(constant)]);
  if (TREE_CODE(constant) == INTEGER_CST) {
    printf("%d\n", TREE_INT_CST_VALUE(constant));
    return;
  }
  printf("()\n");
}
void dumpv_decl_ref(tree decl) {
  printf("%s%d %s ", indent.c_str(), TREE_CODE(decl), tree_code_name[TREE_CODE(decl)]);
  const char* label = (DECL_NAME(decl) ? IDENTIFIER_POINTER(DECL_NAME(decl)) : "(unnamed)");
  location_t loc = DECL_SOURCE_LOCATION(decl);
  printf("%s %s:%d:%d\n", label, LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc));
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

