import elasticapm
import contextvars
import os

client = elasticapm.Client()

def start_submission():
    """
    向 APM 上报开始一个提交的评测

    :returns 该提交对应的上下文对象，需要传递给其他的函数使用
    """
    ctx = contextvars.copy_context()
    def do_submission():
        return client.begin_transaction('Submission')
    ctx.run(do_submission)
    return ctx

def start_judge_task(ctx, name):
    """
    向 APM 上报开始一个评测任务

    :param ctx: 该提交对应的 contextvar 对象，由 start_submission 函数生成
    :param name: 该评测子任务的类型识别名，用于监控系统查看使用，如 "内存测试", "标准测试"
    :returns 该评测任务对应的 span 对象，并在评测子任务完成后传给 end_judge_task
    """
    def make_span():
        span = elasticapm.capture_span(name, leaf=True)
        span.__enter__()
        return span
    return ctx.run(make_span)

def end_judge_task(ctx, span):
    """
    向 APM 上报当前提交评测已经完成

    :param ctx: 该提交对应的 contextvar 对象，由 start_submission 函数生成
    :param span: 该评测子任务对应的 span 对象，由 start_judge_task 函数生成
    """
    def do_span():
        span.__exit__(None, None, None)
    return ctx.run(do_span)

def end_submission(ctx, name, result):
    """
    向 APM 上报当前提交评测已经完成

    :param ctx: 该提交对应的 contextvar 对象，由 start_submission 函数生成
    :param name: 该提交的分类，如 "moj"/"mcourse"/"sicily"
    :param result: 该提交的评测结果，如 "Accepted"
    """
    def do_submission():
        client.end_transaction(name, result)
    return ctx.run(do_submission)

def report_crash(message):
    client.capture_message(message)
