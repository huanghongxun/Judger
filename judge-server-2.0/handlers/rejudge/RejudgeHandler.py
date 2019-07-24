import json
import tornado.web
from utils.logger.loggerUtils import getLogger
from utils.mysql.mysqller import getDatabase
from utils.rabbitmq.rabbitmqer import submissionMQ

submission_log = getLogger("server", "[RejudgeHandler]")
matrix_database = getDatabase("Matrix_database")

GET_SUBMISSION_SQL = """SELECT sub_id, submission.prob_id as prob_id, config, detail, updated_at, ptype_id, is_standard
                        FROM submission, library_problem
                        WHERE sub_id = %(sub_id)s AND submission.prob_id=library_problem.prob_id
                     """

class RejudgeHandler(tornado.web.RequestHandler):

    def get(self):
        sub_id = None
        try:
            sub_id = self.get_argument('sub_id', None)
        except Exception as e:
            submission_log.warn("Request missing sub_id")
            return
            
        result = matrix_database.query(GET_SUBMISSION_SQL, sub_id=sub_id)
        if not result or len(result) == 0:
            submission_log.warn("sub_id not found")
            return

        body = {
            "submissionId": sub_id,
            "standardId": result[0]["prob_id"],
            "problemType": result[0]["ptype_id"],
            "submissionType": result[0]["is_standard"],
            "token": ""
        }
        
        try:
            self.genRelative(result[0], body)
        except Exception as e:
            submission_log.error(
                "Can not parse json from detail or json. Error: %s", e)
            self.send("get file structure failed")
        else:
            self.sendToJudge(body)

    def genRelative(self, record, body):
        body.pop("token")
        body["problemType"] = int(record["ptype_id"])
        body["updated_at"] = record["updated_at"]
        for key in ["config", "detail"]:
            if record[key] is not None:
                body[key] = json.loads(record[key])
            else:
                body[key] = None

    def sendToJudge(self, submission):
        """Send submission which doesn't need files downloading to judge mq"""
        submissionMQ.sendMessage(submission)
