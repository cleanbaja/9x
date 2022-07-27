#!/usr/bin/python3

import sys
import os

if len(sys.argv) == 2 and sys.argv[1] == '--global':
    print('WARNING: this will change every source file in the source tree, proceed? (y or n)')
    res = input()
    if res == 'y':
        os.system('find include/ -iname *.h -o -iname *.c | xargs clang-format -i')
        os.system('find src/ -iname *.h -o -iname *.c | xargs clang-format -i')
else:
    os.system('git clang-format --style=Chromium')


