import eventlet
import socketio
import numpy as np
# import cv2
import time
import collections
import config
import logging

logger = logging.getLogger('Socketio.Server')
logger.setLevel(logging.INFO)
handler = logging.StreamHandler()
handler.setFormatter(logging.Formatter(fmt='%(asctime)s:[%(levelname)s][%(process)d][%(name)s] - %(funcName)s:%(lineno)d - %(message)s'))
logger.addHandler(handler)
# logging.basicConfig(level=logging.WARNING, format='%(asctime)s:[%(levelname)s][%(process)d][%(name)s] - %(funcName)s:%(lineno)d - %(message)s')


# sio = socketio.Server(logger=logger, engineio_logger=True, max_http_buffer_size=480*640*4*2)
sio = socketio.Server(logger=False, engineio_logger=False, max_http_buffer_size=480*640*4*2)
app = socketio.WSGIApp(sio, static_files={
    '/': {'content_type': 'text/html', 'filename': 'index.html'}
})

@sio.on('connect')
def connect(sid, environ):
    global count, elapsed_time_queue, start
    logger.info('>>> connect {}'.format(sid))
    for k, v in environ.items():
        logger.info('\t{}:{}'.format(k, v))

    count = 0
    elapsed_time_queue = collections.deque(maxlen=100)
    start = time.time()

@sio.on('message')
def on_message(sid, data):
    global count, elapsed_time_queue, start
    logger.debug('>>> \tmessage: len(data)={}'.format(len(data)))
    #img = np.frombuffer(data, dtype=np.uint8).reshape((480, 640, 4))
    end = time.time()
    elapsed = end - start
    start = time.time()
    elapsed_time_queue.append(elapsed)
    count += 1
    if count%100 == 0:
        logger.info("\t{} fps".format(1.0/np.mean(elapsed_time_queue)))
    return True
#    cv2.imshow('display', img)
#    cv2.waitKey(1)

@sio.on('disconnect')
def disconnect(sid):
    logger.info('>>> disconnect ', sid)

if __name__ == '__main__':
    logger.info('python-socket.io server')
    eventlet.wsgi.server(eventlet.listen(('', config.PORT)), app)
