import MySQLdb
import json
import sys


db = MySQLdb.connect("localhost", "root", "123", "matrix")
cursor = db.cursor()
q_SQL = """
 select sub_id, report
 from submission natural join library_problem
 where ptype_id = 1 and grade = -1 and submit_at > '2018-01-07 19:00'
"""
m_SQL = """
 update submission
 set grade = {grade}
 where sub_id = {sub_id}
"""
count = 1
cursor.execute(q_SQL)
data = cursor.fetchall()
# print data
for sub_id, raw_report in data:
    try:
        report = json.loads(raw_report)["report"]
        grade = 0
        for answer in report:
            grade += answer["grade"]
        try:
            cursor.execute(m_SQL.format(grade=grade, sub_id=sub_id))
            db.commit()
        except Exception as e:
            db.rollback()
            print(sub_id, grade, file=sys.stderr)
        else:
            print(count, sub_id, grade)
            count += 1
    except Exception as e:
        print(sub_id, e)
