import socket
import stun
import netifaces
import sys
import threading

import argparse
import logging


logger = logging.getLogger('STUN/UDP Peer')
logger.setLevel(logging.INFO)
handler = logging.StreamHandler()
handler.setFormatter(logging.Formatter(fmt='%(asctime)s:[%(levelname)s][%(name)s] - %(message)s'))
logger.addHandler(handler)

def main():
    parser = argparse.ArgumentParser(description='This is toy peer client to test STUN')
    parser.add_argument('-H', '--host', action='store', type=str, default='localhost', help='type signaling server ip address')
    parser.add_argument('-p', '--port', action='store', type=int, default=5000, help='type signaling server port')
    parser.add_argument('-d', '--data-port', action='store', type=int, default=50001, help='type data port for the other peer')
    options = parser.parse_args(sys.argv[1:])
    logger.info("options={}".format(options))

    #
    # references
    #  - https://stackoverflow.com/questions/30698521/python-netifaces-how-to-get-currently-used-network-interface
    #  - https://pypi.org/project/netifaces/
    #
    default_gateway_iface = netifaces.gateways()['default'][netifaces.AF_INET][1]
    logger.info(f"default_gateway_iface={default_gateway_iface}")

    for iface in netifaces.interfaces():
        ifaddresses = netifaces.ifaddresses(iface)
        ret = ifaddresses.get(netifaces.AF_INET, None)
        if ret and iface == default_gateway_iface:
            logger.info(f"default_ip=[{iface}] {ret[0]['addr']}")
            default_ip = ret[0]['addr']

    nat_type, external_ip, external_port = stun.get_ip_info(
                                                    stun_host="stun.l.google.com",
                                                    stun_port=19302,
                                                    source_ip=default_ip,
                                                    source_port=options.data_port)

    logger.info(f"nat_type={nat_type}")
    logger.info(f"external_ip={external_ip}")
    logger.info(f"external_port={external_port}")

    logger.info('connecting to signaling server')
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', options.data_port))
    sock.sendto('{} {}'.format(external_ip, external_port).encode(), (options.host, options.port))

    while True:
        data = sock.recv(1024).decode()

        if data.strip() == 'ready':
            logger.info('checked in with server, waiting')
            break

    data = sock.recv(1024).decode()
    ip, sport = data.split(' ')
    sport = int(sport)

    logger.info('\ngot peer')
    logger.info('  ip:          {}'.format(ip))
    logger.info('  source port: {}'.format(sport))

    # punch hole
    # equiv: echo 'punch hole' | nc -u -p 50001 x.x.x.x 50002
    logger.info('punching hole')

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', options.data_port))
    sock.sendto(b'0', (ip, sport))

    logger.info('ready to exchange messages\n')

    # listen for
    # equiv: nc -u -l 50001
    def listen():

        while True:
            data = sock.recv(1024)
            print('\rpeer: {}\n> '.format(data.decode()), end='')

    listener = threading.Thread(target=listen, daemon=True);
    listener.start()

    # send messages
    # equiv: echo 'xxx' | nc -u -p 50002 x.x.x.x 50001

    while True:
        msg = input('> ')
        sock.sendto(msg.encode(), (ip, sport))

if __name__ == "__main__":
    main()