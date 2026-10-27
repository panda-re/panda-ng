from cffi import FFI
from os.path import join, realpath
import gdb
import re
import tree_sitter_c as tsc
from tree_sitter import Language, Parser
import functools
from glob import glob
from pathlib import Path

C_LANGUAGE = Language(tsc.language())
parser = Parser(C_LANGUAGE)

def traverse_tree(tree):
	cursor = tree.walk()

	visited_children = False
	while True:
		if not visited_children:
			yield cursor.node
			if not cursor.goto_first_child():
				visited_children = True
		elif cursor.goto_next_sibling():
			visited_children = False
		elif not cursor.goto_parent():
			break

def remove_functions(code):
	tree = parser.parse(bytes(code, 'utf8'))
	remove_lines = []
	for node in traverse_tree(tree):
		if node.type == 'function_definition':
			remove_lines.append((node.start_point[0], node.end_point[0]))
		if node.type == 'function_declarator':
			if not node.is_named:
				continue
			if node.text.startswith(b'__assert_fail ') or node.text.startswith(b'__assert_perror_fail ') or node.text.startswith(b'__assert '):
				remove_lines.append((node.start_point[0], node.end_point[0]))
	
	codelines = code.split("\n")
	code = code.split("\n")
	newcode = []
	for line in range(len(code)):
		if any([start <= line <= end for start, end in remove_lines]):
			continue
		else:
			newcode.append(codelines[line])
	return "\n".join(newcode)

@functools.cache
def gdb_resolve_issue(code):
	cmd = code.decode(errors="ignore").replace('\n',' ')
	print(f"cmd={cmd}")
	cmd = cmd.replace("__fd_mask", "unsigned int")	
	try:
		result = gdb.execute(f"p {cmd}",to_string=True).split(" = ")[1].strip()
	except gdb.error:
		# The expression names a host type the debug info lacks (e.g. ELF_NGREG's
		# struct user_regs_struct on aarch64 hosts): ask the host compiler instead.
		result = host_eval(cmd)
	return result.encode()

def host_eval(expr):
	from subprocess import check_output
	from tempfile import TemporaryDirectory
	glib = check_output(['pkg-config', '--cflags', 'glib-2.0']).decode().strip()
	with TemporaryDirectory() as d:
		with open(join(d, "eval.c"), "w") as f:
			f.write('#include "includes/sys_includes.h"\n#include <stdio.h>\n'
				f'int main(void) {{ printf("%lld", (long long)({expr})); return 0; }}\n')
		check_output(f"gcc -I{script_dir} {glib} {join(d, 'eval.c')} -o {join(d, 'eval')}", shell=True)
		return check_output([join(d, "eval")]).decode().strip()
	
def simplify_brackets(code):
	code_bytes = bytes(code, 'utf8')
	tree = parser.parse(code_bytes)
	regex_replace = {}
	for node in traverse_tree(tree):
		if node.type == 'array_declarator':
			if nn := node.children_by_field_name("size"):
				if nn[0].type in ['number_literal', 'identifier']:
					continue
				elif nn[0].type in ['binary_expression', 'parenthesized_expression', 'conditional_expression', 'sizeof_expression']:
					missing = nn[0].text
					regex_replace[missing] = gdb_resolve_issue(missing)
					print(f"Replacing {missing} with {regex_replace[missing]}")
				else:
					breakpoint()
		elif node.type == 'enumerator':
			if nn := node.children_by_field_name("value"):
				if nn[0].type in ['unary_expression', 'conditional_expression', 'parenthesized_expression']:
					missing = nn[0].text
					regex_replace[missing] = gdb_resolve_issue(missing) 
					print(f"Replacing {missing} with {regex_replace[missing]}")
				elif nn[0].type in ['number_literal', 'binary_expression', 'identifier', 'char_literal']:
					 continue
				else:
					breakpoint()
	
	for c in sorted(regex_replace, key=lambda x: -len(x)):
		code_bytes = code_bytes.replace(c, regex_replace[c])
	return code_bytes.decode("utf8", errors="ignore")

def get_functions(prefixes):
	functions = set()
	for x in prefixes:
		g = gdb.execute(f"info functions {x}", to_string=True) 
		for f in g.split("\n"):
			if f.endswith(";"):
				functions.add(f.split(":")[1].strip())
	return functions

def replace_cpp_issues(total):
	total = total.replace("class", "_class")
	total = total.replace("typename", "_typename")
	return total

# Fixup 
def fixup_empty_structs(text):
	pattern = re.compile(r"struct\s{}\s*(?P<name>\S*);")
    # Find all matches in the current string
	matches = pattern.finditer(text)
	# Replace each match
	for m in matches:
		name = m.group('name')
		replacement = f"char {name}[0];"
		text = text.replace(m.group(0), replacement)
	return text

def arch_to_generic(arch):
	if arch == "x86_64":
		return "i386"
	elif "mips" in arch:
		return "mips"
	elif arch == "aarch64":
		return "arm"
	elif arch == "ppc64":
		return "ppc"
	elif arch == "loongarch64":
		return "loongarch"
	elif "riscv" in arch:
		return "riscv"
	return arch

# QEMU names its per-host directories (tcg/<host>, host/include/<host>) after uname -m
host_arch = __import__("platform").machine()

def preprocess(arch, source, include_defines=False): #,fake_sysroot=True):
	from subprocess import check_output
	from os.path import realpath
	from os import getcwd
	from tempfile import TemporaryDirectory
	
	includes = [
		"panda/include",
		"build",
		"include",
		f"target/{arch_to_generic(arch)}",
		f"include/tcg/{arch}",
		f"tcg/{host_arch}",
		f"host/include/{host_arch}/",
		"include/tcg",
		"contrib/plugins",
		"include/qemu",
		realpath(join(script_dir)),
		".",
	]

	glib_includes = check_output(['pkg-config', '--cflags-only-I', 'glib-2.0']).decode('utf-8', errors='ignore').strip()

	includes = " ".join([f"-I{realpath(join(root, include))}" for include in includes])
	includes += ' ' + glib_includes

	defines = '-dD' if include_defines else ''

	# A private directory, not a fixed /tmp/simple.{c,out}: concurrent runs (e.g. engine builds
	# without a nix sandbox) would overwrite each other's files.
	with TemporaryDirectory(prefix="libpanda-ng-") as tmp:
		simple_c, simple_out = join(tmp, "simple.c"), join(tmp, "simple.out")
		with open(simple_c,"w") as f:
			f.write(source)
		cmd = f"gcc -E {defines} -pthread -Wno-unused-result -Wsign-compare -DNDEBUG -g -fwrapv -O3 -Wall -fPIC -UNDEBUG {includes} {simple_c} -o {simple_out} {'-m64' if host_arch == 'x86_64' else ''} -Wall -Winvalid-pch -std=gnu11 -O2 -g -fstack-protector-strong -Wempty-body -Wendif-labels -Wexpansion-to-defined -Wformat-security -Wformat-y2k -Wignored-qualifiers -Wimplicit-fallthrough=2 -Winit-self -Wmissing-format-attribute -Wmissing-prototypes -Wnested-externs -Wold-style-declaration -Wold-style-definition -Wredundant-decls -Wshadow=local -Wstrict-prototypes -Wtype-limits -Wundef -Wvla -Wwrite-strings -Wno-missing-include-dirs -Wno-psabi -Wno-shift-negative-value"
		print(cmd)
		check_output(cmd, shell=True)
		return open(simple_out).read()

# pandare2 loads panda_python_<arch>.h into cffi in ABI mode: cffi computes every struct layout
# itself, from the declarations alone, and has no __attribute__. cffi_layout rewrites the three GNU
# attributes that change a layout into declarations cffi lays out the way gcc does, and drops all
# other attributes. tests/static/layout.py (the flake check `layout`) compares the result with gcc.
#   aligned(N) on a field:          a zero-length array of an N-aligned type before the field
#   aligned(N) on a struct/union:   the same array as its first member (also for a typedef of one)
#   packed on a top-level struct:   the declaration between "// cffi-packed-begin" and
#                                   "// cffi-packed-end" lines; pandare_build.py gives that part to
#                                   cffi with packed=True
#   packed on a typedef'd enum:     its constants as an anonymous enum, and the typedef names the
#                                   integer type gcc gives it (the enum's size from the DWARF)
# What cffi can't express raises, so the header build fails instead of the layout drifting.
CFFI_PACKED_BEGIN = "// cffi-packed-begin"
CFFI_PACKED_END = "// cffi-packed-end"
ALIGN_TYPES = {1: "uint8_t", 2: "uint16_t", 4: "uint32_t", 8: "uint64_t", 16: "long double"}
TOKEN = re.compile(r"""(?P<str>"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|(?P<id>[A-Za-z_]\w*)|(?P<num>\d[\w.]*)|(?P<ws>\s+)|(?P<p>.)""", re.S)

def c_tokens(code):
	return [(m.lastgroup, m.group(), m.start(), m.end()) for m in TOKEN.finditer(code) if m.lastgroup != "ws"]

def matching(toks, i):
	"""Index of the bracket closing the one at toks[i]."""
	pair = {"(": ")", "{": "}", "[": "]"}[toks[i][1]]
	depth = 0
	for j in range(i, len(toks)):
		if toks[j][1] == toks[i][1]:
			depth += 1
		elif toks[j][1] == pair:
			depth -= 1
			if depth == 0:
				return j
	raise Exception(f"unbalanced {toks[i][1]} at offset {toks[i][2]}")

def layout_attributes(code, toks, i, j):
	"""The layout part of the attribute toks[i..j] (__attribute__((...)))): {"aligned": N, "packed": True}."""
	out = {}
	inner = code[toks[i + 1][3]:toks[j][2]].strip()
	assert inner.startswith("(") and inner.endswith(")"), inner
	inner = inner[1:-1]
	items, depth, cur = [], 0, ""
	for ch in inner:
		if ch == "," and depth == 0:
			items.append(cur); cur = ""
			continue
		depth += ch == "("
		depth -= ch == ")"
		cur += ch
	items.append(cur)
	for item in items:
		item = item.strip()
		name = re.match(r"\w*", item).group().strip("_")
		args = item[len(re.match(r"\w*", item).group()):].strip()
		if name == "aligned":
			if not args:
				n = int(host_eval("__BIGGEST_ALIGNMENT__"))
			elif re.fullmatch(r"\(\s*\d+\s*\)", args):
				n = int(args.strip("() "))
			else:
				n = int(gdb_resolve_issue(args.encode()).decode(), 0)
			out["aligned"] = max(n, out.get("aligned", 1))
		elif name == "packed":
			out["packed"] = True
	return out

_markers = __import__("itertools").count()

def align_marker(n):
	if n not in ALIGN_TYPES:
		raise Exception(f"cffi has no {n}-byte aligned type for aligned({n})")
	return f"{ALIGN_TYPES[n]} __cffi_align{n}_{next(_markers)}[0]; "

def packed_enum_type(name):
	"""The integer type gcc gives a packed enum, from the engine's DWARF."""
	t = gdb.lookup_type(name).strip_typedefs()
	if t.code != gdb.TYPE_CODE_ENUM:
		raise Exception(f"{name} is not an enum in the engine's debug info")
	signed = any(f.enumval < 0 for f in t.fields())
	return ("int" if signed else "uint") + f"{8 * t.sizeof}_t"

def cffi_layout(code):
	toks = c_tokens(code)
	edits = []        # (start, end, text), applied back to front
	bodies = []       # every struct, union and enum body: a dict, see below
	statements = []   # top-level declarations: (start, end)
	# A frame per open brace. kind: top, struct, union, enum or other (a function body or an
	# initializer). stmt_start: where the declaration being read at this level starts.
	stack = [{"kind": "top", "stmt_start": 0, "stmt_tok": None, "last_closed": None}]
	pending = None      # after struct/union/enum, until its body opens: the body-to-be
	just_closed = None  # the body whose } was the last token
	i = 0
	while i < len(toks):
		kind, text, start, end = toks[i]
		frame = stack[-1]
		if text in ("__attribute__", "__attribute"):
			j = matching(toks, i + 1)
			attrs = layout_attributes(code, toks, i, j)
			edits.append((start, toks[j][3], ""))
			i = j + 1
			if not attrs or any(f["kind"] == "other" for f in stack):
				continue
			if pending is not None:              # struct ATTR [tag] {
				pending["attrs"].update(attrs)
			elif just_closed is not None:        # struct {...} ATTR
				just_closed["attrs"].update(attrs)
			elif frame["kind"] in ("struct", "union"):   # a field's attribute
				if attrs.get("packed"):
					raise Exception(f"packed on a field: {code[frame['stmt_start']:end]!r}")
				edits.append((frame["stmt_start"], frame["stmt_start"],
					align_marker(attrs["aligned"])))
			elif frame["kind"] == "top" and frame["stmt_tok"] == "typedef" and frame["last_closed"]:
				frame["last_closed"]["attrs"].update(attrs)   # typedef struct {...} Name ATTR;
				frame["last_closed"]["typedef_attrs"] = True
			# Anything else (a function, a variable, a typedef of a scalar) is dropped. A scalar
			# typedef cffi would need more alignment for shows up in tests/static/layout.py.
			continue
		just_closed = None
		if frame["stmt_tok"] is None:
			frame["stmt_tok"] = text
		if text in ("struct", "union", "enum"):
			pending = {"kind": text, "attrs": {}, "tag": None, "tag_span": None, "depth": len(stack)}
		elif text == "{":
			body = pending or {"kind": "other", "attrs": {}, "tag": None, "tag_span": None, "depth": len(stack)}
			pending = None
			body.update(open=end, stmt_start=end, stmt_tok=None, last_closed=None,
				nested=any(f["kind"] == "other" for f in stack))
			stack.append(body)
		elif text == "}":
			body = stack.pop()
			body["close"] = end
			pending = None
			if body["kind"] != "other":
				bodies.append(body)
				just_closed = body
				stack[-1]["last_closed"] = body
			elif stack[-1]["kind"] == "top" and not (i + 1 < len(toks) and toks[i + 1][1] == ";"):
				stack[-1].update(stmt_start=end, stmt_tok=None, last_closed=None)  # a function body
		elif text == ";":
			if frame["kind"] == "top":
				statements.append((frame["stmt_start"], end))
			frame.update(stmt_start=end, stmt_tok=None, last_closed=None)
			pending = None
		elif pending is not None:
			if kind == "id" and pending["tag"] is None:
				pending["tag"] = text
				pending["tag_span"] = (start, end)
			else:
				pending = None   # struct tag used, not defined
		i += 1

	def statement_of(body):
		for s, e in statements:
			if s <= body["open"] < e:
				return s, e
		raise Exception(f"no top-level declaration around offset {body['open']}")

	packed_statements = set()
	for b in bodies:
		a = b["attrs"]
		if not a or b["nested"]:
			continue
		if b["kind"] == "enum":
			if not a.get("packed"):
				continue
			s, e = statement_of(b)
			m = re.fullmatch(r"\s*typedef\s+enum\s*(\{.*\})\s*(\w+)\s*;",
				strip_edits(code, s, e, edits), re.S)
			if b["depth"] != 1 or not m:
				raise Exception(f"packed enum not of the form 'typedef enum {{...}} Name;': {code[s:e]!r}")
			edits = [x for x in edits if not s <= x[0] < e]
			edits.append((s, e, f"\nenum {m.group(1)};\ntypedef {packed_enum_type(m.group(2))} {m.group(2)};"))
			continue
		if a.get("packed") and a.get("aligned"):
			raise Exception(f"packed and aligned on one {b['kind']}: cffi can't express it")
		if a.get("aligned"):
			edits.append((b["open"], b["open"], align_marker(a["aligned"])))
			# On the typedef's name the alignment is the typedef's, not struct <tag>'s: cffi can't
			# tell them apart, so the tag goes when nothing else names it.
			if b.get("typedef_attrs") and b["tag"] and \
					len(re.findall(rf"\b{b['kind']}\s+{b['tag']}\b", code)) == 1:
				edits.append((*b["tag_span"], ""))
		if a.get("packed"):
			s, e = statement_of(b)
			if b["depth"] != 1 or any(o is not b and s <= o["open"] < e for o in bodies):
				raise Exception(f"packed {b['kind']} inside or around another: cffi can't express it: {code[s:e]!r}")
			packed_statements.add((s, e))
	for s, e in packed_statements:
		edits.append((s, s, f"\n{CFFI_PACKED_BEGIN}\n"))
		edits.append((e, e, f"\n{CFFI_PACKED_END}\n"))
	# At one offset: insertions before a removal, and an end marker before a begin marker (two
	# packed declarations in a row).
	order = {CFFI_PACKED_END: 0, CFFI_PACKED_BEGIN: 2}
	out, pos = [], 0
	for s, e, t in sorted(edits, key=lambda x: (x[0], x[1], order.get(x[2].strip(), 1))):
		out += [code[pos:s], t]
		pos = e
	code = "".join(out) + code[pos:]
	# unsigned/__signed__ __int128 (python_fixes.h makes __int128 a 16-byte union)
	return re.sub(r"\b(?:unsigned|signed|__signed__)\s+__cffi_int128\b", "__cffi_int128", code)

def strip_edits(code, s, e, edits):
	"""code[s:e] with the attribute removals in it applied."""
	out, pos = "", s
	for es, ee, t in sorted(x for x in edits if s <= x[0] < e):
		out += code[pos:es] + t
		pos = ee
	return out + code[pos:e]

break_header = "int breakheader();"

def compile_target(arch, target):
	source = f"""
	#include "includes/sys_includes.h"
	{break_header}
	#define COMPILING_PER_TARGET 1
	#define CONFIG_PLUGIN 1
	#define CONFIG_TARGET "{arch}-{target}-config-target.h"
	#define CONFIG_DEVICES "{arch}-{target}-config-devices.h"
	#include CONFIG_TARGET
	#include CONFIG_DEVICES
	#include "config-host.h"
	#include "qemu/compiler.h"
	#include "qemu/host-utils.h"
	#include "qemu/osdep.h"
	#include "exec/hwaddr.h"
	#include "exec/cpu-common.h"
	#include "exec/translation-block.h"
	#include "target/{arch_to_generic(arch)}/cpu-param.h"
	#include "exec/target_long.h"
	#include "system/runstate.h"
	#if __has_include("system/arch_init.h") /* gone in QEMU 11 */
	#include "system/arch_init.h"
	#endif
	#include "system/memory.h"
	#include "system/address-spaces.h"
	#include "system/iothread.h"
	#include "qemu/main-loop.h"
	#include "qemu/thread.h"
	#include "exec/cpu-common.h"
	#include "exec/hwaddr.h"
	#include "exec/memattrs.h"

	#include "qemu/plugin.h"
	#include "qemu/plugin-event.h"
	#include "plugins/qemu-plugin.h"
	#include "plugins/plugin.h"
	#include "panda/types.h"
	#include "panda/callbacks/cb-defs.h"
	#include "panda/plugin.h"
	#include "panda/common.h"
	#include "panda/panda_api.h"
	#include "panda/panda_qemu_plugin_helpers.h"
	"""
	
	c_cpp_source = preprocess(arch, source)
	target_info = f"""
	{break_header}
	#include "{arch}-{target}-config-target.h"
	#include "cpu-param.h"
"""	
	target_defines = just_defines(remove_break_header(preprocess(arch, target_info, include_defines=True)))
	handle_python(arch, source)
	handle_c(arch, c_cpp_source, target_defines)
	handle_cpp(arch, c_cpp_source, target_defines)

def remove_hash_lines(total):
	return "\n".join([x for x in total.split("\n") if x and not x.startswith("#")])

def just_defines(total):
	return "\n".join([x for x in total.split("\n") if x.startswith("#define")])

def remove_break_header(total):
	lines = total.split("\n")
	l = 0
	for line in range(len(lines)):
		if break_header in lines[line]:
			l = line
			break
	total = "\n".join(lines[l+1:])
	return total

def handle_c(arch, total, target_defines):
	# total = preprocess(arch, total)
	total = remove_break_header(total)
	with open(f"panda_c_{arch}.h","w") as f:
		total = remove_functions(remove_hash_lines(total))
		f.write(total)
		f.write("\n")
		f.write(target_defines)

def handle_cpp(arch, total, target_defines):
	# total = preprocess(arch, total)
	total = remove_break_header(total)
	with open(f"panda_cpp_{arch}.h","w") as f:
		total = remove_functions(remove_hash_lines(total))
		total = total.replace("*class;", "*_class;")
		total = total.replace("void aarch64_set_svcr(CPUARMState *env, uint64_t new, uint64_t mask);", "void aarch64_set_svcr(CPUARMState *env, uint64_t _new, uint64_t mask);")
		total = total.replace("*typename", "*_typename")
		total = total.replace("vaddr vaddr;", "vaddr _vaddr;")
		total = fixup_empty_structs(total)
		f.write(total)
		f.write("\n")
		f.write(target_defines)

def ppp_cb_typedef_regex():
	return re.compile(r"PPP_CB_TYPEDEF\( *(void|bool|int) *, *([a-zA-Z0-9_-]+) *, *(.*)\).*")

def typedef(ret_type, name, args):
	return f"typedef {ret_type} (*{name}_t)({args});"

pypanda_start_pattern = """// BEGIN_PYPANDA_NEEDS_THIS -- do not delete this comment bc pypanda
// api autogen needs it.  And don't put any compiler directives
// between this and END_PYPANDA_NEEDS_THIS except includes of other
// files in this directory that contain subsections like this one.
"""

pypanda_end_pattern = "// END_PYPANDA_NEEDS_THIS -- do not delete this comment!\n"

def trim_pypanda(contents):
	'''
	Trim data between pypanda_start_pattern/pypanda_end_pattern
	return None if patterns aren't found
	'''
	a = contents.find(pypanda_start_pattern)
	if a == -1: return None
	a += len(pypanda_start_pattern)
	b = contents.find(pypanda_end_pattern)
	if b == -1: return None
	recurse = None
	if len(contents[b+len(pypanda_end_pattern):]):
		recurse = trim_pypanda(contents[b+len(pypanda_end_pattern):])
	if recurse:
		return contents[a:b]+recurse
	else:
		return contents[a:b]

def copy_ppp_header(filepath):
	# For the PPP-like headers we look for typedefs and then make the void ppp_add_cb(name)(name_t); functions
	# and the bool ppp_remove_cb(name)(name_t)
	# This probably won't support everything
	# pypanda_h = os.path.join(INCLUDE_DIR_PYP, os.path.split(filename)[-1])
	# print("Creating pypanda PPP header [%s] for [%s]" % (pypanda_h, filename))
	new_contents = [f"//Autogenerated PPP header from {filepath}"]
	reg = ppp_cb_typedef_regex()
	contents = open(filepath).read()
	subcontents = trim_pypanda(contents)
	for line in subcontents.split("\n"):
		# now add void ppp_add_cb_{cb_name}({cb_name}_t);
		forbidden = ["#ifndef", "#endif"]
		if any(i in line for i in forbidden):
			continue
		m = reg.match(line)
		if m:
			ret_type = m.groups(1)[0]
			name = m.groups(1)[1]
			args = m.groups(1)[2]
			new_contents.append(typedef(ret_type, name, args))
			new_contents.append(f"void ppp_add_cb_{name}({name}_t);")
			new_contents.append(f"bool ppp_remove_cb_{name}({name}_t);")
			# void ppp_add_cb_{cb_name}(void (*)({cb_args}))
		elif "PPP_CB_TYPEDEF" in line:
			raise Exception(f"Failed to parse: {line}")
		else:
			new_contents.append(line.strip())
	return "\n".join(new_contents)

def handle_python(arch, total):
	total = f"""
	#include "includes/python_fixes.h"
	{total}
	"""

	total = preprocess(arch, total)
	total = "\n".join([x for x in total.split("\n") if x and not x.startswith("#")])
	total = cffi_layout(total)
	total = fixup_empty_structs(total)
	total = remove_functions(total)
	total = simplify_brackets(total)
	total = total.replace("typedef __signed__ ", "typedef ")
	with open(f"panda_python_{arch}.h","w") as f:
		f.write(total)

def get_current_file():
	f = gdb.execute("info files", to_string=True)
	return f.split("Local exec file:")[1].split("',")[0].strip()[1:]
	

def get_context():
	curfile = get_current_file()
	regex = r"libpanda-(?P<arch>\S*)-(?P<target>softmmu|linux-user).so"
	s = re.search(regex, curfile)
	arch, target = s.group("arch"), s.group("target")
	bits = gdb.execute("ptype target_ulong", to_string=True)
	if "unsigned long" in bits:
		bits = 64
	else:
		bits = 32
	return {"arch": arch, 
		 	"target": target, 
			"bits": bits, 
			"file": curfile}

class ExtractTypes(gdb.Command):
	def __init__(self):
		super(ExtractTypes, self).__init__("extract_types", gdb.COMMAND_DATA)

	def invoke(self, arg, from_tty):
		global root
		args = arg.split(" ")
		root = args[0]
		global libpanda
		libpanda = args[1]
		global script_dir
		script_dir = args[2]
		global file_out
		gdb.execute("set pagination off")
		ctx = get_context()
		compile_target(ctx["arch"], ctx["target"])

		gdb.execute("q 100")

ExtractTypes()