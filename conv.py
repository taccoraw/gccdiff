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


file_cache = {}
def flc2pos(loc_file, loc_line, loc_column):
    if loc_file not in file_cache:
        with open(loc_file, 'rb') as f:
            acc = [0] + [len(l) for l in f]
        for i in range(1, len(acc)):
            acc[i] += acc[i-1]
        file_cache[loc_file] = acc
    loc_line = int(loc_line)
    loc_column = int(loc_column)
    return file_cache[loc_file][loc_line-1] + loc_column-1


def main():
    filename_main = sys.argv[1]
    filename_ind = filename_main + '.ind'
    ind_stack = [-1, 0]
    ele_stack = [ET.Element('tree', {'type': TYPE_ROOT}), None]
    with open(filename_ind) as f:
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
            if loc != LOC_UNKNOWN:
                pos = flc2pos(loc_file, loc_line, loc_column)
                length = 1
            else:
                pos = 0
                length = 2
            ele = ET.SubElement(ele_stack[-1], 'tree', {
                'type': type_,
                'typeLabel': typeLabel,
                'label': label,
                'loc_file': loc_file,
                'loc_line': loc_line,
                'loc_column': loc_column,
                'pos': str(pos),
                'length': str(length),
                })
            ele_stack.append(ele)
    ET.ElementTree(ele_stack[0]).write(filename_main+'.gcc')


if __name__ == '__main__':
    main()
