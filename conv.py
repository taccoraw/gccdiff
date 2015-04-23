import sys
import xml.etree.ElementTree as ET


TYPE_LABEL_V_HAS_LOC = {
    'function_decl', 'result_decl', 'parm_decl', 'var_decl', 'bind_expr',
    'call_expr', 'addr_expr', 'return_expr', 'modify_expr', 'cond_expr',
    'eq_expr', 'mult_expr', 'plus_expr', 'decl_expr', 'nop_expr',
    }
MAX_TREE_CODES = 302
CUSTOM_TYPE_START = 500
TYPE_ROOT = str(CUSTOM_TYPE_START + 0)
LABEL_EMPTY = '()'
LABEL_UNNAMED = '(unnamed)'
LOC_UNKNOWN = '(null):0:0'


def main():
    filename = sys.argv[1]
    ind_stack = [-1, 0]
    ele_stack = [ET.Element('tree', {'type': TYPE_ROOT}), None]
    with open(filename) as f:
        for l in f:
            ind = len(l) - len(l.lstrip())
            while ind <= ind_stack[-1]:
                ind_stack.pop()
                ele_stack.pop()
            ind_stack.append(ind)
            tokens = l.split()
            type_, typeLabel = tokens[0], tokens[1]
            if typeLabel in TYPE_LABEL_V_HAS_LOC:
                label = tokens[2]
                loc = tokens[3]
            else:
                label = ' '.join(tokens[2:])
                loc = LOC_UNKNOWN
            if label == LABEL_EMPTY:
                label = ''
            loc_file, loc_line, loc_column = loc.split(':')
            ele = ET.SubElement(ele_stack[-1], 'tree', {
                'type': type_,
                'typeLabel': typeLabel,
                'label': label,
                'loc_file': loc_file,
                'loc_line': loc_line,
                'loc_column': loc_column,
                })
            ele_stack.append(ele)
    ET.ElementTree(ele_stack[0]).write(filename+'.gcc')


if __name__ == '__main__':
    main()
