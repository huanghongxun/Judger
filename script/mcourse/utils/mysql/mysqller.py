import MySQLdb
import time
from datetime import datetime
from utils.logger.loggerUtils import getLogger
from utils.common.configuration import g_config

class Mysqller():
    def __init__(self, host, user, passwd, db, port=3306, timeout=120):
        self.reconnect_time = time.time() + timeout
        self.connection_info = {
            "host": host,
            "user": user,
            "passwd": passwd,
            "db": db,
            "port": port,
            "charset": "ascii",
        }
        self.timeout = timeout
        prefix = "[MySQL] DB={0}".format(db)
        self.log = getLogger("server", prefix)
        self.connect()

    def connect(self):
        """Connect to database."""
        self.log.info("Connect to mysql server %s.", self.connection_info["host"])
        self.conn = MySQLdb.connect(**self.connection_info)

    def execute(self, sql, *args, **kwargs):
        """Execute sql query, and save cursor to self.cursor
        if kwargs is None:
            query sql % args
        else:
            query sql % kwargs
        using Like torndb.
        """
        # self.log.debug(self.reconnect_time - time.time())
        if self.reconnect_time < time.time():
            self.log.debug("Last connecting time: %s.", datetime.fromtimestamp(self.reconnect_time - self.timeout))
            self.conn.close()
            self.connect()
            self.log.info("Reconnect database %s.", self.connection_info["db"])
        self.reconnect_time = time.time() + self.timeout
        cursor = self.conn.cursor()
        try:
            ret = cursor.execute(sql, kwargs or args)
        except Exception as e:
            self.log.debug(kwargs)
            self.log.debug("Execute sql :\"%s\"", sql % kwargs)
            self.log.error("Execute sql failed. Error: %s", e)
        else:
            return ret, cursor
        finally:
            cursor.close()
            self.conn.commit()

    def query(self, sql, *args, **kwargs):
        """Query from database and return a list of dict
        which keys are the column names.
        """
        result = []
        ret_code, cursor = self.execute(sql, *args, **kwargs)
        head = [field[0] for field in cursor.description]
        for row in cursor:
            # mapping column
            record = dict(zip(head, row))
            result.append(record)
        return result

    def update(self, sql, *args, **kwargs):
        ret = self.execute(sql, *args, **kwargs)
        return ret

    def insert(self, sql, *args, **kwargs):
        ret, cursor = self.execute(sql, *args, **kwargs)
        return ret, cursor.lastrowid

def getDatabase(name):
    """Get Mysqller objcet by name.
    The name is the database section in config file
    """
    return Mysqller(g_config.get(name, "server"),
                    g_config.get(name, "user"),
                    g_config.get(name, "password"),
                    g_config.get(name, "database"))
