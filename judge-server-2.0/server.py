# !/usr/bin/env python3

import tornado.ioloop
import tornado.web
import tornado.httpserver
import utils.logger.config as logConfig
import utils.logger.loggerUtils as logger
import sys

from handlers.main.MainHandler import MainHandler
from handlers.submission.SubmissionHandler import SubmissionHandler

if len(sys.argv) != 3:
    sys.stderr.write('{0}: 2 argument needed, {1} given\n'.format(sys.argv[0], len(sys.argv) - 1))
    print("Usage: {0} [port] [config file]".format(sys.argv[0]))
    sys.exit(2)

application = tornado.web.Application([
    (r"/", MainHandler),
    (r"/submission", SubmissionHandler)
])

if __name__ == "__main__":
    port = int(sys.argv[1])
    access_log = logger.tornadoLogAdapter()
    server_log = logger.getLogger("server", "[Server] Main")
    application.settings["log_function"] = access_log
    try:
        application.listen(port)
    except OSError as error:
        server_log.error("Server can not listen port %d. What: %s", port, error)
    except Exception as error:
        server_log.critical("Server got Unexcepted Error. What: %s", error)
    else:
        server_log.debug("Server running on port %d.", port)
        tornado.ioloop.IOLoop.current().start()
