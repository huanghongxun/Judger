#!/usr/bin/env python3
import xmltodict
import json
import sys

'''
转换 Google Test 输出的脚本，返回值：

0: 内存测试通过，将返回 Accepted
1: 内部错误，返回 Compare Error
2: 内存测试未通过，返回 Wrong Answer
'''

if len(sys.argv) != 3:
    sys.stderr.write('{0}: 2 arguments needed, {1} given\n'.format(sys.argv[0], len(sys.argv) - 1))
    print("Usage: {0} [valgrind xml output] [parsed valgrind report json]".format(sys.argv[0]))
    sys.exit(2)

xml_file = open(sys.argv[1], 'r')
result_file = open(sys.argv[2], 'w')

xml_result = xmltodict.parse(xml_file.read())
try:
    result = xml_result["valgrindoutput"]["error"]
    if not isinstance(result, list):
        result = [result]
    for err in result:
        for item in err["stack"]["frame"]:
            item.pop("obj", None)
            item.pop("dir", None)
    json.dump(result_file, indent=4)
    sys.exit(1)
except KeyError as e:
    if e.args[0] == 'error':
        sys.exit(0)
    else:
        sys.stderr.write('Internal error: {0} is not found during parsing memory check result\n'.format(e.args[0]))
        sys.exit(2)
