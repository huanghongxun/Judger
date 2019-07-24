import json
import pika
import time
from utils.common.configuration import g_config
from utils.logger.loggerUtils import getLogger

class RabbitMQer(object):
    def __init__(self, host, port, queue_name, exchange, exchange_type, routing_key, timeout=50):
        prefix = "[RabbitMQ] QUEUE={0}".format(routing_key)
        self.host = host
        self.port = port
        self.log = getLogger("server", prefix)
        self.queue_name = queue_name
        self.exchange = exchange
        self.exchange_type = exchange_type
        self.routing_key = routing_key
        self.exchange = exchange
        self.timeout = timeout

        retry_time = 5
        while retry_time > 0:
            try:
                self.connect()
            except Exception as e:
                if (retry_time > 1):
                    self.log.warn("Can not connect to rabbitmq server. Retry after 2 second.")
                    time.sleep(2)
                else:
                    self.log.error("Can not connect to rabbitmq server. Error: %s.", e)
            else:
                self.log.info("Connect to rabbitmq server.")
                self.last_connect_time = time.time()
                break
            finally:
                retry_time -= 1

    def connect(self):
        self.conn = pika.BlockingConnection(pika.ConnectionParameters(host=self.host, port=self.port))
        self.channel = self.conn.channel()
        self.channel.exchange_declare(exchange=self.exchange, durable=True, exchange_type=self.exchange_type)
        self.channel.queue_declare(queue=self.queue_name, durable=True, exclusive=False, auto_delete=False)
        self.channel.queue_bind(exchange=self.exchange, queue=self.queue_name)

    def sendMessage(self, json_obj):
        body = json.dumps(json_obj, ensure_ascii=False, default=str)
        if self.last_connect_time + self.timeout < time.time():
            self.connect()
        try:
            self.channel.basic_publish(exchange=self.exchange, routing_key=self.routing_key, body=body)
        except Exception as e:
            self.log.debug("SendMessage: %s", body)
            self.log.error("Senf message error. Error: %s", e)
        self.last_connect_time = time.time()

submissionMQ = RabbitMQer(g_config.get("RabbitMQ", "hostname"),
                          g_config.getint("RabbitMQ", "port"),
                          g_config.get("RabbitMQ", "queue_name"),
                          g_config.get("RabbitMQ", "exchange"),
                          g_config.get("RabbitMQ", "exchange_type"),
                          g_config.get("RabbitMQ", "routing_key"))
