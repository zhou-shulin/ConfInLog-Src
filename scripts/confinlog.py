#!/usr/bin/python
#-*-coding:utf-8-*-

import sys
import os
import argparse
import re
import nlp_match as nm
import extract_json as ej

OUTPUT_DIR = "/root/output/"
CONFINLOG_BIN_PATH = '/root/confinlog/build/confinlog'
COMPILED_FILE_FILE_PATH = '/home/work_shop/compiled_files_dir/'



src_path_dict = {
    'httpd':'/home/work_shop/httpd-2.4.43/', 
    'nginx':'/home/work_shop/nginx-1.18.0/', 
    'postgresql':'/home/work_shop/postgresql-11.8/', 
    'mysql':'/home/work_shop/mysql-5.7.29/', 
    'lighttpd':'/home/work_shop/lighttpd-1.4.55/'
}

json_path_dict = {
    'httpd':'/home/work_shop/httpd-2.4.43/compile_commands.json', 
    'nginx':'/home/work_shop/nginx-1.18.0/compile_commands.json', 
    'postgresql':'/home/work_shop/postgresql-11.8/compile_commands.json', 
    'mysql':'/home/work_shop/mysql-5.7.29/build/compile_commands.json', 
    'lighttpd':'/home/work_shop/lighttpd-1.4.55/compile_commands.json'
}

def preprocessPhase(target_software_name):
    if os.path.exists('%s/%s-compiled_files.def'%( COMPILED_FILE_FILE_PATH, target_software_name)):
        return

    if not os.path.exists(json_path_dict[target_software_name]):
        print("Json file NOT found.\n")
        print("Should be at %s.\n"%json_path_dict[target_software_name])
        print("Generate it by bear or cmake..\n")
        return 1

    ej.extract_compiled_files(target_software_name, json_path_dict[target_software_name])


def callASTAnalysis(target_software_name):

    exec_confinlog_cmd = 'cat %s/%s-compiled_files.def | xargs %s -p %s'%(COMPILED_FILE_FILE_PATH, target_software_name, CONFINLOG_BIN_PATH, json_path_dict[target_software_name])
    # exec_confinlog_cmd = 'cat %s/%s-compiled_files.def | xargs %s -p %s > %s/log.out'%(COMPILED_FILE_FILE_PATH, target_software_name, CONFINLOG_BIN_PATH, json_path_dict[target_software_name], OUTPUT_DIR)
    print(exec_confinlog_cmd)
    ret = os.system(exec_confinlog_cmd)

    if ret == 0: # exec confinlog successful
        pass
    elif ret>>8 == 1:   # confinlog return 1
        pass
    elif ret>>8 == 255: # confinlog return -1
        pass

def inferConstraint(target_software_name):

    print("=================================================================\n")
    print("                Inferring Constraints...\n")
    print("=================================================================\n")

    log_message_file_path = '%s/%s-filtered-logs.txt'%( OUTPUT_DIR, target_software_name)  
    if os.path.exists(log_message_file_path) == False:
        print('Could NOT find %s file to infer configuration constraint at present.\n'%log_message_file_path)
        print('Please run confinlog with -[m/e/f] at first.\n')
        exit(1)
    raw_data = open(log_message_file_path, 'r').readlines()
    log_info_list = []
    for data in raw_data:
        data = data.strip()
        items = re.split(r'\@\$\@\$\@', data)
        if len(items) == 2:
            log_info_list.append((items[0], items[1]))

    constraint_list = []
    for log_info in log_info_list:
        # print(log_info)
        conf_name = log_info[0]
        log_message = log_info[1]
        if nm.isConstraintDescription(conf_name, log_message) == True:
            if (conf_name, log_message) not in constraint_list:
                constraint_list.append((conf_name, log_message))

    config_constraints_file_path = '%s/%s-constraints.txt'%(OUTPUT_DIR, target_software_name)
    fout = open( config_constraints_file_path, 'w')
    if not fout:
        print('Open output file %s FAILED!\n'%config_constraints_file_path)
        exit(1)
    for conf_name, constraint in constraint_list:
        fout.write('%s:\n\t%s\n'%(conf_name, constraint))
    fout.close()


if __name__ == '__main__':
    
    parser = argparse.ArgumentParser()

    parser.add_argument('target_software_name', help='The name of target software, e.g. httpd, nginx')

    group = parser.add_mutually_exclusive_group()
    group.add_argument('-p', '--preprocess', action='store_true', help='Preprocess compile_commands.json file of target software.')
    group.add_argument('-e', '--extract', action='store_true', help='Extract log messages from source code.')
    group.add_argument('-m', '--mine', action='store_true', help='Mine program items related to configuration parameters.')
    group.add_argument('-f', '--filter', action='store_true', help='Filter log messages related to configuration parameters.')
    group.add_argument('-i', '--infer', action='store_true', help='Infer configuration constraints from log messages.')
    group.add_argument('-a', '--all', action='store_true', help='Run ConfInLog in one command.')

    args = parser.parse_args()

    if args.target_software_name not in src_path_dict:
        print('No source code for software \"%s\" prepared.'%args.target_software_name)
        print('Recommended parameter value is :')
        print('\t', end='')
        key_list = list(src_path_dict.keys())
        for i in range(len(key_list)-1):
            print( key_list[i], end=', ')
        print(key_list[-1])
        exit(1)

    
    f = open('%s/.option.dat'%OUTPUT_DIR, 'w')
    f.write(args.target_software_name+'\n')
    if args.extract == True:
        f.write('e')
        f.close()
    elif args.mine == True:
        f.write('m')
        f.close()
    elif args.filter == True:
        f.write('f')
    else:
        f.write('n')
    f.close()


    if args.preprocess == True:
        preprocessPhase(args.target_software_name)
    elif args.extract == True or args.mine == True or args.filter == True:
        callASTAnalysis(args.target_software_name)
    elif args.infer == True:
        inferConstraint(args.target_software_name)
    elif args.preprocess == False and args.extract == False and args.mine == False and args.filter == False and args.infer == False:
        preprocessPhase(args.target_software_name)
        callASTAnalysis(args.target_software_name)
        inferConstraint(args.target_software_name)
    elif args.all == True:
        preprocessPhase(args.target_software_name)
        callASTAnalysis(args.target_software_name)
        inferConstraint(args.target_software_name)