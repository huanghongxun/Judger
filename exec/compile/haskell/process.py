#!/usr/bin/env python3
import json
import sys

'''
读入 hlint 的测试结果，并提取出报告并写入 score.txt 和 report.txt
'''

if len(sys.argv) != 5:
    sys.stderr.write('{0}: 4 arguments needed, {1} given\n'.format(
        sys.argv[0], len(sys.argv) - 1))
    print("Usage: {0} [pylint json file] [file number] [report file] [score file]".format(
        sys.argv[0]))
    sys.exit(2)

json_file = open(sys.argv[1], 'r')
file_number = int(sys.argv[2])
report_file = open(sys.argv[3], 'w')
score_file = open(sys.argv[4], 'w')


def as_list(obj, field):
    if field not in obj:
        obj[field] = []
    if not isinstance(obj[field], list):
        obj[field] = [obj[field]]


def getNumberOfFilesWithViolations(violations):
    return len(set([obj["path"] for obj in violations]))


def getNumberOfViolationsWithPriority(violations):
    li = [
        {
            "priority": 0,
            "number": 0
        },
        {
            "priority": 1,
            "number": 0
        },
        {
            "priority": 2,
            "number": 0
        },
        {
            "priority": 3,
            "number": 0
        }
    ]
    for obj in violations:
        li[obj["priority"]]["number"] += 1
    return li


TypeToPriorityDict = {
    "Error": 0,
    "Warning": 1,
    "Suggestion": 3
}

try:
    json_result = json.load(json_file)

    violations = []
    for obj in json_result:
        violations.append({
            "path": obj["file"],
            "startLine": obj["startLine"],
            "startColumn": obj["startColumn"],
            "endLine": obj["endLine"],
            "endColumn": obj["endColumn"],
            "rule": obj["hint"],
            "category": obj["severity"],
            "priority": TypeToPriorityDict[obj["severity"]],
            "message": "你可以将 '" + obj["from"] + "' 修改为 '" + obj["to"] + "'"
        })

    result = {
        'summary': {
            'numberOfFiles': file_number,
            'numberOfFilesWithViolations': getNumberOfFilesWithViolations(violations),
            'numberOfViolationsWithPriority': getNumberOfViolationsWithPriority(violations)
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
