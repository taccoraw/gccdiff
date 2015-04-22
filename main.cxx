// vim:et ts=2 sts=2 sw=2
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "function.h"
#include <stdio.h>
#include <string>

#define INSPECT_TREE_CODE(tree) printf("TREE_CODE(" #tree ") = %s\n", tree_code_name[TREE_CODE(tree)])

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
#define SHOW_LOC(decl) printf("%s:%d:%d\n", DECL_SOURCE_FILE(decl), DECL_SOURCE_LINE(decl), DECL_SOURCE_COLUMN(decl))
#define PRINT_LOC(loc) printf("%s:%d:%d\n", LOCATION_FILE(loc), LOCATION_LINE(loc), LOCATION_COLUMN(loc))

void callback_pre_genericize (void *gcc_data, void *user_data)
{
  tree fndecl = (tree)gcc_data;
  tree resdecl = DECL_RESULT(fndecl);
  // SHOW_LOC(fndecl);
  // SHOW_LOC(resdecl);
  PRINT_LOC(DECL_STRUCT_FUNCTION(fndecl)->function_start_locus);
  printf("Func %s : %s\n", IDENTIFIER_POINTER(DECL_NAME(fndecl)), get_type_name(TREE_TYPE(resdecl)));
  for (tree args = DECL_ARGUMENTS(fndecl); args; args = DECL_CHAIN(args)) {
    // SHOW_LOC(args);
    printf("  args %s : %s\n", IDENTIFIER_POINTER(DECL_NAME(args)), get_type_name(TREE_TYPE(args)));
    // INSPECT_TREE_CODE(TREE_TYPE(args));
  }
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
    // tree resdecl = DECL_RESULT(fndecl);
    tree resdecl = TREE_TYPE(TREE_TYPE(fndecl));
    printf("Func %s : x\n", IDENTIFIER_POINTER(DECL_NAME(fndecl)));
    printf("0x%016x\n", resdecl);
    printf("Func %s : %s\n", IDENTIFIER_POINTER(DECL_NAME(fndecl)), get_type_name(resdecl));
    return;
  }
  INSPECT_TREE_CODE(decl);
}

