import logging.handlers
import multiprocessing
from .loggerUtils import getLogger, LogQueue

class LoggerServer:
    """Singleton log server for logging
    """
    def __new__(cls, *args, **kwargs):
        if not hasattr(cls, "_Instance"):
            instance = super(LoggerServer, cls).__new__(cls, *args, **kwargs)
            instance.handlers = {}
            instance._process = None
            instance.queue = LogQueue
            instance.log = getLogger("logging_server", "[Logging]")
            LoggerServer._Instance = instance
            # using share value to ensure safety in all the processes.
        return LoggerServer._Instance

    def addHandler(self, tag, handler):
        """add handler for handle log record
        """
        self.handlers[tag] = handler

    def handle(self, tag, record):
        """handle log record
        """
        if tag in self.handlers:
            self.handlers[tag].handle(record)
        else:
            self.log.warn("Logging server handle an unexpected record")

    def stop(self):
        self.log.info("Stopping logging server.")
        self.queue.put_nowait(None)
        self._process.join()
        self.log.info("Logging server stopped.")

    def start(self):
        if self._process is None:
            self._process = multiprocessing.Process(target=self.run)
            self._process.start()
            self.log.info("Start logging server.")

    def run(self):
        while True:
            task = self.queue.get()
            if task is None:
                self.log.info("Got an empty task. EXIT.")
                break
            else:
                try:
                    self.handle(task["tag"], task["record"])
                    # self.log.debug("Log record to handler %s.", task["tag"])
                except Exception as err:
                    self.log.error("Error in logging server. Err: %s.", err)
                    raise err
