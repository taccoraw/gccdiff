import sys
from subprocess import check_output
import xml.etree.ElementTree as ET
import difflib


EXT0 = '.ind'
EXT1 = '.gcc'


def collect(prefix, out):
    ext = EXT0
    path_list = check_output(['find', prefix, '-name', '*'+ext, '-print0']).rstrip('\0').split('\0')
    for path in path_list:
        assert path.startswith(prefix) and path.endswith(ext)
        path_code = path[:-len(ext)]
        filename = path[len(prefix):-len(ext)]
        with open(path) as f:
            for i, l in enumerate(f):
                assert l != '\n'
                if l[0] in {' ', '\t'}:
                    continue
                tokens = l.split()
                type_, typeLabel = tokens[:2]
                assert typeLabel == 'function_decl'
                label, loc, endLoc = tokens[2:]
                funcname = label
                print >> out, '{} {} {}'.format(filename, funcname, i)


def lookup(filename, funcname, manifest):
    with open(manifest) as f:
        for l in f:
            tokens = l.split()
            if filename == tokens[0] and funcname == tokens[1]:
                return int(tokens[2])
    return -1


MAX_TREE_CODES = 302
CUSTOM_TYPE_START = 500
TYPE_ROOT = str(CUSTOM_TYPE_START + 0)
TYPE_NULL = str(CUSTOM_TYPE_START + 1)
TYPELABEL_NULL = 'NULL_ptr'
LABEL_EMPTY = '()'
LABEL_UNNAMED = '(unnamed)'
LOC_UNKNOWN = '(null):0:0'
LOC_BUILTIN = '<built-in>:0:0'
TREE_NULL = 'NULL_xx'


FUNCFILE = None
file_cache = {}
def flc2pos(loc_file, loc_line, loc_column):
    if loc_file != FUNCFILE:
        return 0
    if not loc_file.startswith(PREFIX_CODE):
        loc_file = PREFIX_BUILD + loc_file
    if loc_file not in file_cache:
        with open(loc_file, 'rb') as f:
            acc = [0] + [len(l) for l in f]
        for i in range(1, len(acc)):
            acc[i] += acc[i-1]
        file_cache[loc_file] = acc
    loc_line = int(loc_line)
    loc_column = int(loc_column)
    return file_cache[loc_file][loc_line-1] + loc_column-1


def gen_gcc(filename, funcname, prefix_code, offset, prefix_build, outname):
    ind_stack = []
    ele_stack = []
    global PREFIX_CODE, PREFIX_BUILD
    PREFIX_CODE = prefix_code
    PREFIX_BUILD = prefix_build
    with open('{}{}{}'.format(prefix_code, filename, EXT0)) as f:
        for i, l in enumerate(f):
            if i < offset:
                continue
            elif i == offset:
                assert l != '\n'
                assert l[0] not in {' ', '\t'}
                tokens = l.split()
                type_, typeLabel = tokens[:2]
                assert typeLabel == 'function_decl'
                label, loc, endLoc = tokens[2:]
                assert label == funcname
                loc_file, loc_line, loc_column = loc.split(':')
                endLoc_file, endLoc_line, endLoc_column = endLoc.split(':')
                assert loc_file == endLoc_file
                global FUNCFILE
                FUNCFILE = loc_file
                ret = (loc_file, loc_line, '1', endLoc_file, endLoc_line, endLoc_column)
                ind_stack = [0]
                ele_stack = [ET.Element('tree', {
                    'type': type_,
                    'typeLabel': typeLabel,
                    'label': label,
                    'loc_file': loc_file,
                    'loc_line': loc_line,
                    'loc_column': loc_column,
                    'pos': str(flc2pos(loc_file, loc_line, loc_column)),
                    'length': str(flc2pos(endLoc_file, endLoc_line, endLoc_column) - flc2pos(loc_file, loc_line, '1')),
                    })]
                continue
            assert l != '\n'
            ind = len(l) - len(l.lstrip())
            if ind == 0:
                break
            while ind <= ind_stack[-1]:
                ind_stack.pop()
                ele_stack.pop()
            ind_stack.append(ind)
            tokens = l.split()
            if len(tokens) == 1 and tokens[0] == TREE_NULL:
                ele_stack.append(ET.SubElement(ele_stack[-1], 'tree', {
                    'type': TYPE_NULL,
                    'typeLabel': TYPELABEL_NULL,
                    'label': '',
                    'loc_file': LOC_UNKNOWN.split(':')[0],
                    'loc_line': LOC_UNKNOWN.split(':')[1],
                    'loc_column': LOC_UNKNOWN.split(':')[2],
                    'pos': ele_stack[-1].get('pos'),
                    'length': ele_stack[-1].get('length'),
                    }))
                continue
            elif len(tokens) == 4 and tokens[2] in {'???', '<unknown>'} and tokens[3][:4] =='TDO_':
                ele_stack.append(ET.SubElement(ele_stack[-1], 'tree', {
                    'type': tokens[0],
                    'typeLabel': tokens[1],
                    'label': '???',
                    'loc_file': LOC_UNKNOWN.split(':')[0],
                    'loc_line': LOC_UNKNOWN.split(':')[1],
                    'loc_column': LOC_UNKNOWN.split(':')[2],
                    'pos': ele_stack[-1].get('pos'),
                    'length': ele_stack[-1].get('length'),
                    }))
                continue
            type_, typeLabel = tokens[0], tokens[1]
            if typeLabel[-5:] in {'_decl', '_expr'}:
                label = tokens[2]
                loc = tokens[3]
            else:
                label = ' '.join(tokens[2:])
                loc = LOC_UNKNOWN
            if label == LABEL_EMPTY:
                label = ''
            loc_file, loc_line, loc_column = loc.split(':')
            if loc not in {LOC_UNKNOWN, LOC_BUILTIN}:
                pos = flc2pos(loc_file, loc_line, loc_column)
                length = 1
            else:
                pos = ele_stack[-1].get('pos')
                length = ele_stack[-1].get('length')
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
    ET.ElementTree(ele_stack[0]).write(outname)
    return ret


def parse_gum(gum):
    start0, end0, start1, end1 = {}, {}, {}, {}
    cur = {'start': start0, 'end': end0}
    for l in gum.splitlines():
        if l == '-----':
            cur = {'start': start1, 'end': end1}
            continue
        pos, start_end, tag = l.split(' ', 2)
        pos = int(pos)
        if pos not in cur[start_end]:
            cur[start_end][pos] = []
        cur[start_end][pos].append(tag)
    return start0, end0, start1, end1


def extract_code(filename, loc, prefix_code):
    loc_file, loc_line, loc_column, endLoc_file, endLoc_line, endLoc_column = loc
    with open('{}{}'.format(prefix_code, filename), 'rb') as f:
        line_list = f.readlines()
    del line_list[int(endLoc_line):]
    line_list[-1] = line_list[-1][:int(endLoc_column)]
    del line_list[:int(loc_line)-1]
    return line_list


def process_line(num, text, pos, start, end):
    html = ''
    if isinstance(num, int):
        text = text.replace('\0+', '\2').replace('\0-', '\3').replace('\0^', '\4')
        for c in text:
            assert c != '\0'
            if c == '\1':
                html += '</span>'
            elif c == '\2':
                html += '<span class="diff_add">'
            elif c == '\3':
                html += '<span class="diff_sub">'
            elif c == '\4':
                html += '<span class="diff_chg">'
            else:
                html += ''.join(start.get(pos, []))
                html += {'&':'&amp;','<':'&lt;','>':'&gt;'}.get(c, c)
                pos += 1
                html += ''.join(end.get(pos, []))
    return html, pos


def merge_mark(gum_dict, diff, loc0, loc1):
    pos0 = flc2pos(loc0[0], loc0[1], loc0[2])
    pos1 = flc2pos(loc1[0], loc1[1], loc1[2])
    start0, end0, start1, end1 = gum_dict
    '''
    print '<table>'
    for (num0, text0), (num1, text1), flag in diff:
        html0, pos0 = process_line(num0, text0, pos0, start0, end0)
        html1, pos1 = process_line(num1, text1, pos1, start1, end1)
        print '<tr><td>{}</td><td>{}</td><td><pre>{}</pre></td><td>{}</td><td><pre>{}</pre></td></tr>'.format(flag, num0, html0, num1, html1)
    print '</table>'
    '''


def gen_html(filename, funcname):
    offset0 = lookup(filename, funcname, '/tmp/V0')
    offset1 = lookup(filename, funcname, '/tmp/V1')
    assert offset0 != -1 and offset1 != -1
    loc0 = gen_gcc(filename, funcname, V0_CODE, offset0, V0_BUILD, TMP_V0_GCC+EXT1)
    loc1 = gen_gcc(filename, funcname, V1_CODE, offset1, V1_BUILD, TMP_V1_GCC+EXT1)
    gum = check_output(['java', '-jar', '/home/wxjin11/gumtree/client/target/gumtree.jar', '-o', 'tag', TMP_V0_GCC, TMP_V1_GCC])
    print gum
    gum_dict = parse_gum(gum)
    code0 = extract_code(filename, loc0, V0_CODE)
    code1 = extract_code(filename, loc1, V1_CODE)
    diff = list(difflib._mdiff(code0, code1))
    merge_mark(gum_dict, diff, loc0, loc1)
    # from pprint import pprint
    # pprint(filter(lambda x: x[2], diff))
    # _ = difflib.HtmlDiff()
    # print _.make_file(code0, code1, '{}/{}'.format(filename, funcname), '{}/{}'.format(filename, funcname))


V0_CODE = '/tmp/linux-3.8.12/'
V0_BUILD = '/tmp/build-l8p-allno/'
V0_MANIFEST = '/tmp/V0'
V1_CODE = '/tmp/linux-3.8.13/'
V1_BUILD = '/tmp/build-l8-allno/'
V1_MANIFEST = '/tmp/V1'
TMP_V0_GCC = '/tmp/tmp0.c'
TMP_V1_GCC = '/tmp/tmp1.c'


def main():
    # src_dir = sys.argv[1]
    # dst_dir = sys.argv[2]
    '''
    with open(V0_MANIFEST, 'w') as out:
        collect(V0_CODE, out)
    with open(V1_MANIFEST, 'w') as out:
        collect(V1_CODE, out)
    print 'Done.'
    '''
    gen_html('arch/x86/kernel/cpu/perf_event_intel_lbr.c', 'branch_type')


if __name__ == '__main__':
    main()
