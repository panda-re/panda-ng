from cffi import FFI
from os.path import join, realpath
import gdb
import re
import tree_sitter_c as tsc
from tree_sitter import Language, Parser
import functools
from glob import glob
from pathlib import Path

root = "../../qemu"
C_LANGUAGE = Language(tsc.language())
parser = Parser(C_LANGUAGE)

plugin_dir = realpath("../../plugins")


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
	result = gdb.execute(f"p {cmd}",to_string=True).split(" = ")[1].strip()
	return result.encode()
	
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

def arch_to_generic(arch):
	if arch == "x86_64":
		return "i386"
	elif "mips" in arch:
		return "mips"
	elif arch == "aarch64":
		return "arm"
	elif arch == "ppc64":
		return "ppc"
	return arch

def preprocess(arch, source, include_defines=False): #,fake_sysroot=True):
	from subprocess import check_output
	from os.path import realpath
	
	includes = [
		"panda/include",
		"build",
		"include",
		"/usr/include/glib-2.0",
		"/usr/lib/x86_64-linux-gnu/glib-2.0/include",
		f"target/{arch_to_generic(arch)}",
		f"include/tcg/{arch}",
		f"tcg/i386",
		f"host/include/i386/",
		"include/tcg",
		realpath("."),
		realpath("../.."),
		plugin_dir,
		".",
		"contrib/plugins",
		"include/qemu",
	]

	
	with open("/tmp/simple.c","w") as f:
		f.write(source)
	includes = " ".join([f"-I{realpath(join(root, include))}" for include in includes])

	defines = '-dD' if include_defines else ''

	cmd = f"/usr/bin/gcc -E {defines} -pthread -Wno-unused-result -Wsign-compare -DNDEBUG -g -fwrapv -O3 -Wall -fPIC -UNDEBUG {includes} /tmp/simple.c -o /tmp/simple.out -m64 -Wall -Winvalid-pch -std=gnu11 -O2 -g -fstack-protector-strong -Wempty-body -Wendif-labels -Wexpansion-to-defined -Wformat-security -Wformat-y2k -Wignored-qualifiers -Wimplicit-fallthrough=2 -Winit-self -Wmissing-format-attribute -Wmissing-prototypes -Wnested-externs -Wold-style-declaration -Wold-style-definition -Wredundant-decls -Wshadow=local -Wstrict-prototypes -Wtype-limits -Wundef -Wvla -Wwrite-strings -Wno-missing-include-dirs -Wno-psabi -Wno-shift-negative-value"
	print(cmd)
	check_output(cmd, shell=True)
	
	total = open("/tmp/simple.out").read()
	return total

def process_line(line):
	import re
	line = re.sub(r'__attribute__\s*\(\(.*?\)\)', '', line)
	return line


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
	#include "target/{arch_to_generic(arch)}/cpu-param.h"
	#include "exec/target_long.h"
	#include "system/runstate.h"
	#include "system/arch_init.h"

	#include "qemu/plugin.h"
	#include "qemu/plugin-event.h"
	#include "qemu/qemu-plugin.h"
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
	syscalls2_lookup = {
		"i386": "syscalls_ext_typedefs_x86.h",
		"x86_64": "syscalls_ext_typedefs_x64.h",
		"arm": "syscalls_ext_typedefs_arm.h",
		"aarch64": "syscalls_ext_typedefs_arm64.h",
		"mips": "syscalls_ext_typedefs_mips.h",
		"mipsel": "syscalls_ext_typedefs_mips.h",
		"mips64": "syscalls_ext_typedefs_mips64.h",
		"mips64el": "syscalls_ext_typedefs_mips64.h",
	}
	ffibuilder = FFI()
	total = f"""
	#include "includes/python_fixes.h"
	{total}
	#include "includes/python_plugin_includes.h"
	"""

	syscalls2_val = syscalls2_lookup.get(arch, None)
	if syscalls2_val:
		total += copy_ppp_header(join(plugin_dir, f"syscalls2/generated/{syscalls2_lookup[arch]}"))
	total += copy_ppp_header(join(plugin_dir, "syscalls2/generated/syscalls_ext_typedefs.h"))
	total += copy_ppp_header(join(plugin_dir, "osi/os_intro.h"))

	int_fns = glob(f"{plugin_dir}/**/*_int_fns.h", recursive=True)
	for ints in int_fns:
		pdir = Path(ints).relative_to(plugin_dir)
		total += f'#include "{pdir}"\n'
	total = preprocess(arch, total)
	total = "\n".join([process_line(x) for x in total.split("\n") if x and not x.startswith("#")])
	total = remove_functions(total)
	total = simplify_brackets(total)
	with open(f"panda_python_{arch}.h","w") as f:
		f.write(total)
	mod = f"_pandare_ffi_{arch}_softmmu"
	ffibuilder.set_source(mod, None)
	ffibuilder.cdef(total, override=True)
	ffibuilder.compile(debug=True)


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
		global file_out
		gdb.execute("set pagination off")
		ctx = get_context()
		compile_target(ctx["arch"], ctx["target"])

ExtractTypes()