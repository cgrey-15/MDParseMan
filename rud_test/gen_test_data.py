import argparse
import re
import glob
import subprocess

def main():
    
    cl_parser = argparse.ArgumentParser(prog='gen_test_data', description='Generates text with both original text and html output result.\n')
    cl_parser.add_argument('-d', '--dir', default='./')
    cl_parser.add_argument('-o', '--out', default='./')
    cl_parser.add_argument('-c', '--command-dir', default = '')
    cl_parser.add_argument('name', nargs='+')
    args = vars(cl_parser.parse_args())
    #print(args)

    cmd_name = set_cmd_name_to_use(args['command_dir'])

    for classname in args['name']:
        matches = re.fullmatch('([^#,]+)((#+)|(,[1-9][0-9]?))', classname)
        if matches.group(4) == None:
            lengthy = len(matches.group(3))
        else:
            lengthy = int(matches.group(4)[1:])
        i_start = len(matches.group(1))

        globbi = '' + args['dir'] + matches.group(1) + ('[0-9]'*lengthy) + '.txt'


        paths = glob.glob(globbi)

        #print(paths)
        #print(globbi)

        for el in paths:
            #print(el)
            i_name = 0
            i_a = el.rfind('/')
            i_b = el.rfind('\\')
            if not i_a < 0 or not i_b < 0:
                if i_a > i_b:
                    i_name = i_a + 1
                else:
                    i_name = i_b + 1
            #if i_name < 0:
            #    i_name = el.rfind('\\')
            #if i_name < 0:
            #    i_name = 0
            #else:
            #    i_name = i_name+1

            #print(args['out'] + '/' + matches.group(1) + el[i_name+i_start: i_name+i_start + lengthy] + '.txt.out')
            out_file = open(args['out'] + '/' + matches.group(1) + el[i_name+i_start: i_name+i_start + lengthy] + '.txt.out', mode='w')
            res = subprocess.run([cmd_name[1] + cmd_name[0], el], text=True, capture_output=True)

            in_file = open(el)
            for l in in_file:
                out_file.write(l)
            in_file.close()
            out_file.write('\\\\\\__OUT_FILE:\n')
            out_file.write(res.stdout)
            out_file.close()

    return 0

def set_cmd_name_to_use(raw_name):
    cmd_name = ''
    cmd_prefix = ''
    if raw_name != '':
        if raw_name[-1] == '/' or raw_name[-1] == '\\':
            cmd_name = 'cmark'
            cmd_prefix = raw_name
        else:
            i = raw_name.rfind('/')
            if i < 0:
                i = raw_name.rfind('\\')
            if i >= 0:
                cmd_name = raw_name[i+1:]
                cmd_prefix = raw_name[:i+1]
            else:
                cmd_name = raw_name
    else:
        cmd_name = 'cmark'
    return (cmd_name, cmd_prefix)

main()
