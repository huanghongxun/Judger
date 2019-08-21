import configparser
import os
import sys
from utils.logger.loggerUtils import getLogger

g_config = configparser.ConfigParser()
log = getLogger("server", "[Configuration]")
g_config.read(sys.argv[1])