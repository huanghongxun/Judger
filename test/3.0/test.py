import json
import pika
import sys
import MySQLdb

if len(sys.argv) != 3:
    sys.stderr.write('{0}: 2 arguments needed, {1} given\n'.format(sys.argv[0], len(sys.argv) - 1))
    print("Usage: {0} [request.json] [queue_name]".format(sys.argv[0]))
    sys.exit(2)

host = "127.0.0.1"
port = 25565
queue_name = sys.argv[2]
exchange = "Submission"

db = MySQLdb.connect('localhost', 'test', 'test', 'matrix')

def init_rabbitmq():
    global host, port, queue_name, exchange
    connection = pika.BlockingConnection(pika.ConnectionParameters(host, port))
    channel = connection.channel()
    channel.exchange_declare(exchange=exchange, durable=True,
                             exchange_type="direct")
    channel.queue_declare(queue=queue_name, durable=True)
    channel.queue_bind(exchange=exchange, queue=queue_name)
    return channel

def main():
    channel = init_rabbitmq()
    req = json.load(open(sys.argv[1]))
    sub_id = req['sub_id']
    prob_id = req['prob_id']

    cursor = db.cursor()
    cursor.execute('DROP TABLE IF EXISTS submission')
    cursor.execute("""CREATE TABLE `submission` (
        `sub_id` int(11) NOT NULL AUTO_INCREMENT,
        `prob_id` int(11) NOT NULL,
        `grade` int(11) DEFAULT '-1',
        `is_aborted` tinyint(4) DEFAULT '0',
        PRIMARY KEY (`sub_id`)
    ) DEFAULT CHARSET=utf8""")

    cursor.execute('INSERT INTO submission (sub_id, prob_id) VALUES ({0}, {1})'.format(sub_id, prob_id))
    db.commit()
    db.close()

    global queue_name, exchange
    channel.basic_publish(exchange=exchange, routing_key=queue_name, body=json.dumps(req))
    channel.start_consuming()

if __name__ == "__main__":
    main()
