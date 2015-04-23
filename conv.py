import sys
import xml.etree.ElementTree as ET


MAX_TREE_CODES = 302
CUSTOM_TYPE_START = 500
TYPE_ROOT = str(CUSTOM_TYPE_START + 0)


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
            type_, typeLabel, label = tokens[0], tokens[1], ' '.join(tokens[2:])
            if label == '()':
                label = ''
            ele = ET.SubElement(ele_stack[-1], 'tree', {
                'type': type_,
                'typeLabel': typeLabel,
                'label': label,
                })
            ele_stack.append(ele)
    ET.ElementTree(ele_stack[0]).write(filename+'.gcc')


if __name__ == '__main__':
    main()
