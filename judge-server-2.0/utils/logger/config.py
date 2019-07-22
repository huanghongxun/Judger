import logging
import logging.config
import logging.handlers
import multiprocessing
import tornado.log
import sys
import os
from .loggerServer import LoggerServer, LogQueue

LOG_PATH = ["logging_logs", "download_logs", "server_logs"]
for sub_path in LOG_PATH:
    log_path = os.path.join("logs", sub_path)
    if os.path.isdir(log_path) is False:
        os.makedirs(log_path)

logger_config = {
    "version": 1,
    "formatters": {
        "colored": {
            "()": "colorlog.ColoredFormatter",
            "format": "%(log_color)s%(asctime)s.%(msecs)03d %(levelname)s  %(message)s (%(filename)s:%(lineno)d)%(reset)s",
            "datefmt": "%Y-%m-%d %H:%M:%S",
            "log_colors": {
                "DEBUG":    "blue",
                "INFO":     "green",
                "WARNING":  "yellow",
                "ERROR":    "red",
                "CRITICAL": "red,bg_white",
            },
        },
        "no_colored": {
            "format": "%(asctime)s.%(msecs)03d %(levelname)s  %(message)s (%(filename)s:%(lineno)d)",
            "datefmt": "%Y-%m-%d %H:%M:%S",
        },
    },
    "handlers": {
        "stream": {
            "class": "logging.StreamHandler",
            "stream": sys.stdout,
            "formatter": "colored",
            "level": "DEBUG",
        },
        "server_handler": {
            "class": "logging.handlers.BlockingQueueHandler",
            "level": "DEBUG",
            "queue": LogQueue,
            "tag": "server",
        },
        "download_handler": {
            "class": "logging.handlers.BlockingQueueHandler",
            "level": "DEBUG",
            "queue": LogQueue,
            "tag": "download",
        },
        "logging_handler": {
            "class": "logging.handlers.TimedRotatingFileHandler",
            "filename": "logs/logging_logs/log_server.log",
            "level": "DEBUG",
            "when": "D",
            "backupCount": 50,
        },
    },
    "loggers": {
        "server": {
            "handlers": ["server_handler", "stream"],
            "level": "DEBUG",
        },
        "download": {
            "handlers": ["download_handler", "stream"],
            "level": "DEBUG",
        },
        "logging_server": {
            "handlers": ["logging_handler", "stream"],
        },
    },
}

file_formatter = logging.Formatter(fmt="%(asctime)s.%(msecs)03d %(levelname)s  %(message)s (%(filename)s:%(lineno)d)",
                                   datefmt="%Y-%m-%d %H:%M:%S")

log_server_handlers = {
    "server": logging.handlers.TimedRotatingFileHandler("logs/server_logs/server.log",
                                                        when="D",
                                                        backupCount=50),
    "download": logging.handlers.TimedRotatingFileHandler("logs/download_logs/download.log",
                                                          when="D",
                                                          backupCount=50),
}

def initConfig():
    """Init the logger config and disable native tornado access log
    """
    tornado.log.access_log.propagate = False
    logging.config.dictConfig(logger_config)
    log_server = LoggerServer()
    for tag in log_server_handlers:
        log_server_handlers[tag].setFormatter(file_formatter)
        log_server.addHandler(tag, log_server_handlers[tag])
    log_server.start()
    logging.getLogger("server").propagate = False

initConfig()