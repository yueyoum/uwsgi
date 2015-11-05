#!./uwsgi --http-socket :9090 --module tests.worker_warehouse --master --processes 4 --buffer-size 32768

# /                 return all workers warehouse
# /get              return this worker warehouse
# /set?data=xxx     set xxx in this worker warehouse
# /clean            clean this worker warehouse

### test client ###

# import os
# import base64
# import urllib
#
# import requests
#
# WAREHOUSE = {}
#
#
# # test set
# for i in range(10000):
#     data = urllib.quote_plus(base64.b64encode(os.urandom(1000)))
#     req = requests.get("http://127.0.0.1:9090/set?data=%s" % data)
#     assert req.ok is True
#
#     worker_id = req.content
#     WAREHOUSE[worker_id] = data
#
#     req = requests.get("http://127.0.0.1:9090")
#     server_warehouse = req.json()
#
#     for k, v in WAREHOUSE.iteritems():
#         assert WAREHOUSE[k] == server_warehouse[k]
#
# # test clean
# for i in range(1000):
#     if not WAREHOUSE:
#         break
#
#     req = requests.get("http://127.0.0.1:9090/clear")
#     assert req.ok is True
#
#     worker_id = req.content
#     if worker_id in WAREHOUSE:
#         WAREHOUSE.pop(worker_id)
#
#         req = requests.get("http://127.0.0.1:9090")
#         server_warehouse = req.json()
#
#         assert server_warehouse[worker_id] == ''


import json
import urllib
import urlparse
import uwsgi


def warehouse_get(*args):
    x = uwsgi.warehouse_get()
    uwsgi.log("worker {0} got {1} type {2} ".format(uwsgi.worker_id(), x, type(x)))
    return x

def warehouse_set(*args):
    data = args[0]
    data = int(data)
    uwsgi.warehouse_set(data)
    uwsgi.log("set {0} in worker {1}".format(data, uwsgi.worker_id()))
    return str(uwsgi.worker_id())

def warehouse_clear(*args):
    uwsgi.warehouse_clear()
    uwsgi.log("warehouse clean in worker {0}".format(uwsgi.worker_id()))
    return str(uwsgi.worker_id())

def all_workers_warehouse(*args):
    warehouse = uwsgi.workers_warehouse()
    print type(warehouse), warehouse
    return warehouse


routes = {
    '': all_workers_warehouse,
    '/get': warehouse_get,
    '/set': warehouse_set,
    '/clear': warehouse_clear,
}


def application(env, sr):
    path = env['PATH_INFO'].rstrip('/')

    try:
        handler = routes[path]
    except KeyError:
        sr('404 NOT FOUND', [('Content-Type', 'text/plain')])
        return "NOT FOUND"

    sr('200 OK', [('Content-Type', 'text/plain')])
    try:
        data = urlparse.parse_qs(env['QUERY_STRING'])['data'][0]
        data = urllib.quote_plus(data)
    except:
        data = ""

    value = handler(data)
    if isinstance(value, str):
        return value

    return json.dumps(value)

