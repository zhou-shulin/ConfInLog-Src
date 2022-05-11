#!/usr/bin/python3

import sys
import os
import re


LLVM_BIN = '/home/llvm-10.0.0/build/bin'
LLVM_LIB = '/home/llvm-10.0.0/build/lib'

COMPILED_FILE_FILE_PATH = '/home/work_shop/compiled_files_dir/'



def extract_compiled_files(target_software_name, json_path):
	fout = open('%s/%s-compiled_files.def'%(COMPILED_FILE_FILE_PATH, target_software_name), 'w')

	JSON = open(json_path)
	Lines = JSON.readlines()
	outcommand = ''
	for line in Lines:
		line = line[:-1]
		subflds = line.split(':')
		command = subflds[0].strip()
		command = command.replace('\"','')
		if command  == 'file':
			file = subflds[1]
			file = file.replace('\"', '')
			file = file.replace(' ', '')
			fout.write(file+'\n')
	fout.close()

def generate_ir_build_script(target_software_name, json_path, src_path):
	fout = open('%s/%s_build_ir.sh'%(src_path, target_software_name), 'w')
	fout.write('#!/bin/bash\n')
	fout.write('#this is automatically produced by "extract_json.py compile_commands.json", DO NOT change!\n\n\n ')

	JSON = open(json_path)
	Lines = JSON.readlines()
	outcommand = ''
	for line in Lines:
		line = line[:-1]
		subflds = line.split(':')
		command = subflds[0].strip()
		command = command.replace('\"','')
		if command == 'command':
			compile = subflds[1]
			# compile = compile.replace('\"', '')
			# option = compile.split(' ')
			compile = compile[2:-2]
			# print(compile)
			option = compile.split(' ')
			CC = option[0]
			# CC = option[1]
			if CC == 'gcc' or CC == 'cc' or CC == '/usr/bin/cc':
				outcommand = "\n"+LLVM_BIN+"clang -g -emit-llvm "
			elif CC == 'g++' or CC == 'c++' or CC == '/usr/bin/cc':
				outcommand = "\n"+LLVM_BIN+"clang++ -g emit-llvm "
			else:
				outcommand = "\n"+LLVM_BIN+"clang++ -g emit-llvm "


			for field in option:
				pattern = re.compile(r'-[ID].*')
				match = pattern.match(field)
				if match :
					outcommand = outcommand+ ' '+field +' '
					# print outcommand

		elif command  == 'file':
			file = subflds[1]
			file = file.replace('\"', '')
			file = file.replace(' ', '')

			outcommand = outcommand + ' -c '+ file+' '
			file = file.replace('.cpp', '.cpp.bc')
			file = file.replace('.cc', '.cc.bc')
			file = file.replace('.c', '.c.bc')
			outcommand = outcommand+' -o '+ file
			# print outcommand

			lastfile = ''
			if file != lastfile:
				fout.write('cd '+directory)
				fout.write(outcommand+'\n')
				lastfile = file

		elif command == 'directory':
			directory = subflds[1]
			directory = directory.replace('\"', '')
			directory = directory.replace(' ', '')
			directory = directory[:-1]


	fout.write('\n')
	fout.close()

	os.system('chmod +x %s/%s_build_ir.sh'%(src_path, target_software_name))



if __name__ == '__main__':
	extract_compiled_files('httpd', '/home/work_shop/httpd-2.4.43/compile_commands.json')
	generate_ir_build_script('httpd', '/home/work_shop/httpd-2.4.43/compile_commands.json', '/home/work_shop/httpd-2.4.43/')