import sys
from subprocess import check_output
import xml.etree.ElementTree as ET
import difflib


EXT0 = '.ind'
EXT1 = '.gcc'


def collect(prefix, out):
    ext = EXT0
    path_list = check_output(['find', prefix, '-name', '*'+ext, '-print0']).rstrip('\0').split('\0')
    count = 0
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
                count += 1
    return count


def lookup(filename, funcname, manifest):
    with open(manifest) as f:
        for l in f:
            tokens = l.split()
            if filename == tokens[0] and funcname == tokens[1]:
                return int(tokens[2])
    return -1


MAX_TREE_CODES = 302
CUSTOM_TYPE_START = 500
TYPE_NULL = str(CUSTOM_TYPE_START + 1)
TYPELABEL_NULL = 'NULL_ptr'
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
            assert l[-1] == '\n'
            l = l[:-1]
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
                    'label': '',  # funcname should not be on this node but its child
                    'loc_file': loc_file,
                    'loc_line': loc_line,
                    'loc_column': loc_column,
                    'pos': str(flc2pos(loc_file, loc_line, loc_column)),
                    'length': str(flc2pos(endLoc_file, endLoc_line, endLoc_column) - flc2pos(loc_file, loc_line, '1')),
                    'aux': '',
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
            tokens = l.split(None, 2)
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
                    'aux': '',
                    }))
                continue
            type_, typeLabel = tokens[0], tokens[1]
            if typeLabel.endswith(('_expr', '_ref')):
                label = ''
                assert len(tokens) == 3
                loc, macro = tokens[2].split(None, 1)
                aux = macro
            elif typeLabel.endswith('_decl'):
                label = ''
                loc = LOC_UNKNOWN
                aux = ''
            else:
                assert len(tokens) <= 3
                label = tokens[2] if len(tokens) == 3 else ''
                loc = LOC_UNKNOWN
                aux = ''
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
                'aux': aux,
                })
            ele_stack.append(ele)
    if outname is not None:
        ET.ElementTree(ele_stack[0]).write(outname)
    return ret


def parse_gum(gum):
    from collections import defaultdict as d
    start0, end0, start1, end1 = d(list), d(list), d(list), d(list)
    macro = {}
    uid0, uid1 = d(list), d(list)
    cur = {'start': start0, 'end': end0, 'uid': uid0, 'v': '0'}
    for l in gum.splitlines():
        if l == '-----':
            cur = {'start': start1, 'end': end1, 'uid': uid1, 'v': '1'}
            continue
        uid, start, end, action, mid, title, aux = l.split(' ', 6)
        uid, start, end, = int(uid), int(start), int(end)
        if mid != 'NA':
            mid = int(mid)
            cur['start'][start].append('<span id="map-{}-{}"></span>'.format(cur['v'], mid))
        cur['start'][start].append('<span class="{}" id="act-{}" title="{}" data-map="{}">'.format(action, uid, title, mid))
        cur['end'][end].append('</span>')
        if aux != '' and aux != '()':
            macro[uid] = (aux, cur['v'])
        cur['uid'][start].append(uid)
    return start0, end0, start1, end1, macro, uid0, uid1


def extract_code(filename, loc, prefix_code):
    loc_file, loc_line, loc_column, endLoc_file, endLoc_line, endLoc_column = loc
    with open('{}{}'.format(prefix_code, filename), 'rb') as f:
        line_list = f.readlines()
    del line_list[int(endLoc_line):]
    line_list[-1] = line_list[-1][:int(endLoc_column)]
    del line_list[:int(loc_line)-1]
    return line_list


def process_line(num, text, pos, start, end, uid):
    html = ''
    uids = []
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
                uids += uid.get(pos, [])
    return html, uids, pos


def merge_mark(gum_dict, diff, loc0, loc1):
    global FUNCFILE, PREFIX_CODE, PREFIX_BUILD
    FUNCFILE, PREFIX_CODE, PREFIX_BUILD = loc0[0], V0_CODE, V0_BUILD
    pos0 = flc2pos(loc0[0], loc0[1], loc0[2])
    FUNCFILE, PREFIX_CODE, PREFIX_BUILD = loc1[0], V1_CODE, V1_BUILD
    pos1 = flc2pos(loc1[0], loc1[1], loc1[2])
    start0, end0, start1, end1, macro, uid0, uid1 = gum_dict
    diff_mark = {True:{True:'M',False:'D'},False:{True:'A',False:'?'}}
    print HTML_PRE
    print '<table id="code">'
    for (num0, text0), (num1, text1), flag in diff:
        num0 = int(loc0[1])+num0-1 if isinstance(num0, int) else ''
        num1 = int(loc1[1])+num1-1 if isinstance(num1, int) else ''
        html0, uids0, pos0 = process_line(num0, text0, pos0, start0, end0, uid0)
        html1, uids1, pos1 = process_line(num1, text1, pos1, start1, end1, uid1)
        print '<tr><td>{}</td>'.format(diff_mark[isinstance(num0, int)][isinstance(num1, int)] if flag else '')
        mk = lambda uid: '<span class="mk">'+('<a href="#macro-{i}">{i}</a>'.format(i=uid) if uid in macro else str(uid))+'</span>'
        print '<td>{}</td><td>{}</td><td><pre>{}</pre></td>'.format(', '.join(map(mk, uids0)), num0, html0)
        print '<td>{}</td><td>{}</td><td><pre>{}</pre></td>'.format(', '.join(map(mk, uids1)), num1, html1)
        print '</tr>'
    print '</table>'
    file_cache = {}
    for uid in macro:
        print '<table class="macro" id="macro-{}">'.format(uid)
        print '<caption>Macro Expansion Info for Action #{}</caption>'.format(uid)
        if macro[uid][1] == '0':
            PREFIX_CODE, PREFIX_BUILD = V0_CODE, V0_BUILD
        else:
            PREFIX_CODE, PREFIX_BUILD = V1_CODE, V1_BUILD
        raw = macro[uid][0]
        assert raw[0] == '(' and raw[-1] == ')'
        parts = raw[1:-1].split()
        assert len(parts) % 2 == 1
        for i in range(0, len(parts), 2)[::-1]:
            loc_file, loc_line, loc_column = parts[i].split(':')
            loc_line, loc_column = int(loc_line), int(loc_column)
            if not loc_file.startswith(PREFIX_CODE):
                loc_file = PREFIX_BUILD + loc_file
            if loc_file not in file_cache:
                with open(loc_file) as f:
                    file_cache[loc_file] = f.readlines()
            code = file_cache[loc_file][loc_line-1]
            if i-1 >= 0 and code[loc_column-1:loc_column-1+len(parts[i-1])] == parts[i-1]:
                code = code[:loc_column-1]+'<span class="pos">'+code[loc_column-1:loc_column-1+len(parts[i-1])]+'</span>'+code[loc_column-1+len(parts[i-1]):]
            else:
                code = code[:loc_column-1]+'<span class="pos">'+code[loc_column-1:loc_column]+'</span>'+code[loc_column:]
            print '<tr><td>{}</td><td><pre>{}</pre></td></tr>'.format(parts[i], code)
            # if i-1 >= 0:
            #     print '<tr><td><pre>{}</pre></td><td></td></tr>'.format(parts[i-1])
        print '</table>'
    print HTML_PST


def gen_html(filename, funcname):
    offset0 = lookup(filename, funcname, V0_MANIFEST)
    offset1 = lookup(filename, funcname, V1_MANIFEST)
    assert offset0 != -1 and offset1 != -1
    loc0 = gen_gcc(filename, funcname, V0_CODE, offset0, V0_BUILD, TMP_V0_GCC+EXT1)
    loc1 = gen_gcc(filename, funcname, V1_CODE, offset1, V1_BUILD, TMP_V1_GCC+EXT1)
    gum = check_output(['java', '-jar', '/home/wxjin11/gumtree/client/target/gumtree.jar', '-o', 'tag', TMP_V0_GCC, TMP_V1_GCC])
    # print gum
    gum_dict = parse_gum(gum)
    code0 = extract_code(filename, loc0, V0_CODE)
    code1 = extract_code(filename, loc1, V1_CODE)
    diff = list(difflib._mdiff(code0, code1))
    # print gum_dict
    merge_mark(gum_dict, diff, loc0, loc1)

HTML_PRE = '''\
<style>
table, table td {
	border-collapse: collapse;
	border-left: 1px solid;
	border-right: 1px solid;
}
table {
	border: 1px solid;
}
td {
	text-align: right;
}
pre {
	text-align: left;
	font-size: 12px;
}
.pos {color: red;}
#panel {
	position: fixed;
    top: 0.3em;
	background-color: #c0c0c0;
}
table#code {
    margin-top: 2em;
}
</style>
<style id="css_gum">
.add, .del, .upd, .mv {font-size: 18px;}
.add {color: green;}
.del {color: red;}
.upd {font-weight: bolder;}
.mv  {background-color: #c0c0ff;}
</style>
<style id="css_diff">
.diff_add {text-decoration: underline;}
.diff_sub {text-decoration: line-through;}
.diff_chg {font-style: italic;}
</style>
<div id="panel">
<input type="checkbox" name="box_gum" id="box_gum" />
<label for="box_gum">GumTree</label>
<input type="checkbox" name="box_diff" id="box_diff" />
<label for="box_diff">diff</label>
</div>
'''
HTML_PST = '''\
<script>
main();
var css_gum, css_diff, css_parent;
function main() {
	css_gum = document.querySelector('#css_gum');
	css_diff = document.querySelector('#css_diff');
	css_parent = css_gum.parentElement;
	css_parent.removeChild(css_diff);
	var box_gum = document.querySelector('#box_gum');
	box_gum.onchange = toggle;
	box_gum.checked = true;
	var box_diff = document.querySelector('#box_diff');
	box_diff.onchange = toggle;
	box_diff.checked = false;
}
function toggle() {
	var target;
	if (this.id == 'box_gum')
		target = css_gum;
	else
		target = css_diff;
	if (this.checked)
		css_parent.appendChild(target);
	else
		css_parent.removeChild(target);
}
</script>
'''


#'''
V0_CODE = '/tmp/linux-3.8.12/'
V0_BUILD = '/tmp/build-l8p-allno/'
V0_MANIFEST = '/tmp/V0'
V1_CODE = '/tmp/linux-3.8.13/'
V1_BUILD = '/tmp/build-l8-allno/'
V1_MANIFEST = '/tmp/V1'
TMP_V0_GCC = '/tmp/tmp0.c'
TMP_V1_GCC = '/tmp/tmp1.c'
'''
V0_CODE = '/tmp/sample/old/'
V0_BUILD = '/tmp/sample/'
V0_MANIFEST = '/tmp/sample/V0'
V1_CODE = '/tmp/sample/new/'
V1_BUILD = '/tmp/sample/'
V1_MANIFEST = '/tmp/sample/V1'
TMP_V0_GCC = '/tmp/sample/tmp0.c'
TMP_V1_GCC = '/tmp/sample/tmp1.c'
'''
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
    # gen_html('part.c', 'main')


if __name__ == '__main__':
    main()
