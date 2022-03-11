#!/usr/bin/python3

# A simple CPIO initrd builder (stolen from managarm)

import os
import shutil
import subprocess
import tempfile
import sys
import argparse

class Entry:
	__slots__ = ('is_dir', 'source', 'strip')

	def __init__(self, is_dir=False, source=None, strip=False):
		self.is_dir = is_dir
		self.source = source
		self.strip = strip

file_dict = dict()

def create_cmdline():
	cmfile = open('build/gen/cmdline', 'w')
	cmfile.write('maxsink=2,nopcid')
	cmfile.close()

def add_dir(rel_path):
	file_dict[rel_path] = Entry(is_dir=True)

def add_file(src_prefix, tree_prefix, filename, rename_to=None, strip=False):
	dest_filename = filename if rename_to is None else rename_to
	src_path = os.path.join(src_prefix, filename)
	rel_path = os.path.join(tree_prefix, dest_filename)
	file_dict[rel_path] = Entry(source=src_path, strip=strip)

# Add all the files.
add_dir('boot')
add_dir('boot/kernel')

# Add the kernel
add_file('build', 'boot/kernel', '9x.elf', rename_to='ninex-kernel', strip=True)

# Create and add the cmdline file
create_cmdline()
add_file('build/gen', 'boot', 'cmdline')

# Copy (= hard link) the files to a temporary directory
tree_path = tempfile.mkdtemp(prefix='initrd-', dir='.')
file_list = sorted(file_dict.keys())
for rel_path in file_list:
	entry = file_dict[rel_path]
	dest_path = os.path.join(tree_path, rel_path)

	if entry.is_dir:
		os.mkdir(dest_path)
	elif entry.strip:
		subprocess.check_call([f'strip', '-o', dest_path, entry.source])
	else:
		os.link(entry.source, dest_path)

# create the actual CPIO
"""proc = subprocess.Popen(['cpio', '--create', '--format=newc',
			'-D', tree_path,
			'--file', 'initrd',
			'--quiet'],
		stdin=subprocess.PIPE,
		encoding='ascii')
proc.communicate(input='\n'.join(file_list))
if proc.returncode == 0:
	sys.exit(1)
"""
# create the actual TAR
proc = subprocess.run(["tar", "cf", "initrd", "-C", tree_path, "."])
if proc.returncode != 0:
	sys.exit(1)

# GZIP the initrd image
final_proc = subprocess.run(['gzip', '-S' '.img', 'initrd', '--quiet'])
if final_proc.returncode != 0:
	sys.exit(1)

# Delete the artifacts
shutil.rmtree(tree_path)

