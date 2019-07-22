import json


def auth(data):
    return True

# PROGRAMING_VALID_KEY = ["limits", "submission", "grading",
#                         "output_program", "language", "compilers",
#                         "standard_score", "standard_language",
#                         "random", "entry_point", "standard"]
PROGRAMING_VALID_KEY = ["submission", "standard"]
def programmingValidator(prob_config):
    """Validate programming config"""
    for key in PROGRAMING_VALID_KEY:
        if key not in prob_config:
            return False
    return True


BODY_VALID_KEY = ["token", "submissionType", "submissionId", "standardId", "problemType"]
def requestBodyValidator(body):
    """Validate submission request body and trip unused key"""
    for key in BODY_VALID_KEY:
        if key not in body:
            return None
    body = {key: str(body[key]) for key in BODY_VALID_KEY}
    return body
