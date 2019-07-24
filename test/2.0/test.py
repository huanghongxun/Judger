import MySQLdb
import sys
import json
import time
import requests

if len(sys.argv) != 3:
    sys.stderr.write('{0}: 2 arguments needed, {1} given\n'.format(sys.argv[0], len(sys.argv) - 1))
    print("Usage: {0} [config.json] [detail.json]".format(sys.argv[0]))
    sys.exit(2)

url = "http://localhost:9000/submission"

db = MySQLdb.connect("localhost", "test", "test", "matrix_course")

cursor = db.cursor()
cursor.execute('DROP TABLE IF EXISTS submission')
cursor.execute("""CREATE TABLE `submission` (
  `sub_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `user_id` bigint(20) unsigned NOT NULL,
  `prob_id` bigint(20) unsigned NOT NULL,
  `submit_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `grade` int(4) DEFAULT NULL,
  `report` json DEFAULT NULL,
  `detail` json DEFAULT NULL,
  `is_aborted` tinyint(1) NOT NULL DEFAULT '0',
  `is_standard` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`sub_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8""")
cursor.execute('DROP TABLE IF EXISTS library_problem')
cursor.execute("""CREATE TABLE `library_problem` (
  `prob_id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `lib_id` bigint(20) unsigned NOT NULL,
  `ptype_id` bigint(20) unsigned NOT NULL,
  `updated_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `title` varchar(128) NOT NULL,
  `description` text NOT NULL,
  `config` json NOT NULL,
  `creator` bigint(20) unsigned NOT NULL,
  `is_deleted` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`prob_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8""")
cursor.execute("INSERT INTO library_problem (prob_id, lib_id, ptype_id, title, description, config, creator) VALUES (1234, 0, 0, 'Test', 'Test', '{0}', 1)".format(open(sys.argv[1]).read()))
cursor.execute("INSERT INTO submission (sub_id, user_id, prob_id, detail) VALUES (12340, 1, 1234, '{0}')".format(open(sys.argv[2]).read()))
db.commit()
post_data = {
    "token": "",
    "submissionId": "12340",
    "standardId": "1234",
    "submissionType": "0",
    "problemType": "programming"
}
requests.post(url, data=json.dumps(post_data))

while True:
    cursor.execute("SELECT report FROM submission WHERE prob_id=1234 AND sub_id=12340 AND grade >= 0")
    data = cursor.fetchall()
    
    time.sleep(1)

    if not data:
        continue
    else:
        print(data[0])
        break
