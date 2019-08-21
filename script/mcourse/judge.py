import MySQLdb
import json
import requests
url = "http://localhost:9000/submission"
db = MySQLdb.connect("localhost", "root", "123456", "matrix")
cursor = db.cursor()
SQL = """
 select S.sub_id, S.prob_id
 from submission S, library_problem L
 where S.prob_id = L.prob_id and L.ptype_id = 0 and S.grade is NULL limit 10
"""
# SQL = 'select sub_id, prob_id from submission where sub_id = 22398'
count = 1
cursor.execute(SQL)
data = cursor.fetchall()
# print data
for item in data:
    print(count, item[1], item[0])
    post_data = {
        "token": "",
        "submissionId": str(item[0]),
        "standardId": str(item[1]),
        "submissionType": "0",
        "problemType": "programming"
    }
    requests.post(url, data=json.dumps(post_data))
    count += 1
