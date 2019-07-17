import json
import pika
import sys

if len(sys.argv) != 3:
    sys.stderr.write('{0}: 2 arguments needed, {1} given\n'.format(sys.argv[0], len(sys.argv) - 1))
    print("Usage: {0} [request.json] [queue_name]".format(sys.argv[0]))
    sys.exit(2)

host = "127.0.0.1"
port = 25565
queue_name = sys.argv[2]
exchange = "Submission"

def init_rabbitmq():
    global host, port, queue_name, exchange
    connection = pika.BlockingConnection(pika.ConnectionParameters(host, port))
    channel = connection.channel()
    channel.exchange_declare(exchange=exchange,
                             exchange_type="direct")
    channel.queue_declare(queue=queue_name)
    channel.queue_bind(exchange=exchange, queue=queue_name)
    return channel

def on_response(ch, method, props, body):
    global exchange
    print(body.decode("utf-8"))
    callback_queue = props.reply_to
    print(callback_queue)
    if callback_queue is None:
        return
    input("waiting...")
    ch.basic_publish(exchange=exchange, routing_key=callback_queue, body=body)

def main():
    channel = init_rabbitmq()
    req = json.load(open(sys.argv[1]))
    result = channel.queue_declare(queue="callback_queue", exclusive=True)
    callback_queue = result.method.queue
    global queue_name, exchange
    channel.queue_bind(exchange=exchange, queue=callback_queue)
    channel.basic_consume(on_response, no_ack=True, queue=callback_queue)
    channel.basic_publish(exchange=exchange, routing_key=queue_name,
                          properties=pika.BasicProperties(reply_to=callback_queue), body=json.dumps(req))
    channel.start_consuming()

if __name__ == "__main__":
    main()
