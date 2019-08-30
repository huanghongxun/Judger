#!/usr/bin/env python3
import xmltodict
import json
import sys

'''
读入 checkstyle 的测试结果，并提取出报告并写入 score.txt 和 report.txt
'''

if len(sys.argv) != 5:
    sys.stderr.write('{0}: 4 arguments needed, {1} given\n'.format(
        sys.argv[0], len(sys.argv) - 1))
    print("Usage: {0} [check style xml file] [file number] [report file] [score file]".format(
        sys.argv[0]))
    sys.exit(2)

xml_file = open(sys.argv[1], 'r')
file_number = int(sys.argv[2])
report_file = open(sys.argv[3], 'w')
score_file = open(sys.argv[4], 'w')


def as_list(obj, field):
    if field not in obj:
        obj[field] = []
    if not isinstance(obj[field], list):
        obj[field] = [obj[field]]


severity2priority = {
    'ignore': 3,
    'info': 2,
    'warning': 1,
    'error': 0
}

try:
    xml_result = xmltodict.parse(xml_file.read())
    xml_result = xml_result["checkstyle"]
    as_list(xml_result, "file")

    violations = []
    violatedFiles = 0
    priorities = [0, 0, 0, 0]
    for file in xml_result["file"]:
        if file["error"]:
            violatedFiles += 1
        as_list(file, "error")
        for error in file["error"]:
            violations.append({
                'path': file["@name"],
                'startLine': int(error["@line"]),
                'startColumn': 0,
                'endLine': int(error["@line"]),
                'endColumn': 0,
                'rule': error["@source"],
                'priority': severity2priority[error["@severity"]],
                'message': error["@message"]
            })
            priorities[severity2priority[error["@severity"]]] += 1

    result = {
        'summary': {
            'numberOfFiles': file_number,
            'numberOfFilesWithViolations': violatedFiles,
            'numberOfViolationsWithPriority': [
                {
                    'priority': i,
                    'number': x
                } for i, x in enumerate(priorities)
            ]
        },
        'violation': violations
    }

    grade = 10
    for violation in result["summary"]["numberOfViolationsWithPriority"]:
        grade -= (3 - violation['priority']) * violation['number']
    grade = 0 if grade < 0 else grade
    result["type"] = "oclint"
    json.dump(result, report_file)

    score_file.write('{0} {1}'.format(grade, 10))
    score_file.close()
    report_file.close()

    if grade == 10:
        sys.exit(42)  # Accepted
    elif grade == 0:
        sys.exit(43)  # Wrong Answer
    else:
        sys.exit(54)  # Partial Correct
except KeyError as e:
    sys.stderr.write(
        'Internal error: {0} is not found during parsing memory check result\n'.format(e.args[0]))
    sys.exit(1)  # Internal Error
