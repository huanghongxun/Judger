import json
import tornado.web
from utils.logger.loggerUtils import getLogger
from utils.common import validator
from utils.mysql.mysqller import getDatabase
from utils.rabbitmq.rabbitmqer import submissionMQ

submission_log = getLogger("server", "[SubmissionHandler]")
matrix_database = getDatabase("Matrix_database")

GET_SUBMISSION_SQL = """SELECT sub_id, prob_id, config, detail, updated_at, ptype_id
                        FROM submission NATURAL JOIN library_problem
                        WHERE sub_id = %(sub_id)s AND prob_id = %(prob_id)s AND grade IS NULL
                     """


class SubmissionHandler(tornado.web.RequestHandler):

    def __init__(self, *args, **kwargs):
        super(SubmissionHandler, self).__init__(*args, **kwargs)
        self.is_sent = False

    def set_default_headers(self):
        self.set_header('Content-type', 'application/json;charset=utf-8')

    def post(self):
        body = None
        try:
            body = json.loads(self.request.body.decode())
            body = validator.requestBodyValidator(body)
        except Exception as e:
            submission_log.warn("submission body load failed. Error: %s", e)
        if body is None:
            # invalid body format
            self.send("data format is error")
        elif validator.auth(body["token"]) is False:
            # check auth status failed
            self.send("token invalid")
        else:
            sub_id = body["submissionId"]
            prob_id = body["standardId"]
            submission_log.info(
                "Submission %s with prob_id %s whose type is %s", sub_id, prob_id, body["problemType"])
            result = None
            try:
                result = matrix_database.query(
                    GET_SUBMISSION_SQL, sub_id=sub_id, prob_id=prob_id)
            except Exception as e:
                submission_log.debug(result)
                submission_log.error(
                    "Error when query submission from database. Error: %s", e)

            if result is None or len(result) == 0:
                self.send("query file_structure failed")
                return

            try:
                self.genRelative(result[0], body)
            except Exception as e:
                submission_log.error(
                    "Can not parse json from detail or json. Error: %s", e)
                self.send("get file structure failed")
            else:
                self.sendToJudge(body)

        # if no error msg sent, sending success.
        if self.is_sent is False:
            self.send("none", "yes")

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

    def send(self, msg="unknown error", receive="no", status_code=200):
        if self.is_sent is True:
            return
        if receive == "no":
            submission_log.debug("Send message %s.", msg)
        response = {
            "message": msg,
            "received": receive
        }
        self.write(response)
        self.is_sent = True
