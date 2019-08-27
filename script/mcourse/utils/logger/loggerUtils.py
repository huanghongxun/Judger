import logging
import sys
import logging.handlers
import traceback
import multiprocessing
BlockingQueueHandlerLock = multiprocessing.Lock()
LogQueue = multiprocessing.Queue(1000)


def getLogger(loggerName, prefix=""):
    """Get a logger or a prefixLogger by logger name and prefix
    If there was a specific prefix string, the function would return a
    prefixLogger adapter with the string.
    """
    raw_logger = logging.getLogger(loggerName)
    return PrefixLogger(raw_logger, str(prefix))


class tornadoLogAdapter():
    """A logger adapter handles the access log of tornado.
    To use, tornado.web.Application.settings["log_function"] = tornadoLogAdapter().logger_request
    """
    LEVEL_MAP = [None, None, logging.INFO,
                 logging.INFO, logging.WARN, logging.ERROR]

    def __init__(self):
        self.logger = getLogger("server", "[Server] Access")

    def __call__(self, handler):
        """Logging tornado accessing message according request status.
        Logger http status code mapping table:
        2xx: logging.INFO
        3xx: logging.INFO
        4xx: logging.WARN
        5xx: logging.ERROR
        others: logging.CRITICAL
        """
        level = int(handler.get_status() / 100)
        if level < 2 or level > 5:
            logging.getLogger().critical("tornado got a ridiculous http status code")
        else:
            request_time = handler.request.request_time() * 1000.0
            exc_type, exc_obj, exc_tb = sys.exc_info()
            print('\n'.join(traceback.format_exception(exc_type, exc_obj, exc_tb)))
            self.logger.log(tornadoLogAdapter.LEVEL_MAP[level],
                            "%d %s %.2fms",
                            handler.get_status(),
                            handler._request_summary(),
                            request_time)


class PrefixLogger(logging.LoggerAdapter):
    """A logger inherit from LoggerAdapter.
    The adapter add ``prefix`` string in the log message.
    Format: "%(message)s" -> "prefix | %(message)s"
    """

    def process(self, msg, kwargs):
        return "%s | %s" % (self.extra, msg), kwargs


class BlockingQueueHandler(logging.handlers.QueueHandler):
    """logging handler using blocking queue in multiprocessing
    """

    def __init__(self, tag, queue):
        super(BlockingQueueHandler, self).__init__(queue)
        self.tag = tag
        global BlockingQueueHandlerLock
        self.lock = BlockingQueueHandlerLock

    def prepare(self, record):
        super(BlockingQueueHandler, self).prepare(record)
        return {"tag": self.tag, "record": record}

    def enqueue(self, record):
        self.queue.put(record)


logging.handlers.BlockingQueueHandler = BlockingQueueHandler
