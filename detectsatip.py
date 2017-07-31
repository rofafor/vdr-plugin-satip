#!/usr/bin/env python2
# -*- coding: utf-8 -*-
""" Simple tool to detect SAT>IP devices as JSON.
"""
import json
import socket
import sys
import xml.etree.ElementTree as ET
import requests

SSDP_BIND = '0.0.0.0'
SSDP_ADDR = '239.255.255.250'
SSDP_PORT = 1900
SSDP_MX = 1
SSDP_ST = 'urn:ses-com:device:SatIPServer:1'
SSDP_REQUEST = 'M-SEARCH * HTTP/1.1\r\n' + \
               'HOST: %s:%d\r\n' % (SSDP_ADDR, SSDP_PORT) + \
               'MAN: "ssdp:discover"\r\n' + \
               'MX: %d\r\n' % (SSDP_MX, ) + \
               'ST: %s\r\n' % (SSDP_ST, ) + \
               '\r\n'


def parse_satip_xml(data):
    """ Parse SAT>IP XML data.

    Args:
        data (str): XML input data..

    Returns:
        dict: Parsed SAT>IP device name and frontend information.
    """
    result = {'name': '', 'frontends': {}}
    if data:
        root = ET.fromstring(data)
        name = root.find('.//*/{urn:schemas-upnp-org:device-1-0}friendlyName')
        result['name'] = name.text
        satipcap = root.find('.//*/{urn:ses-com:satip}X_SATIPCAP')
        caps = {}
        for system in satipcap.text.split(","):
            cap = system.split("-")
            if cap:
                count = int(cap[1])
                if cap[0] in caps:
                    count = count + caps[cap[0]]
                caps[cap[0]] = count
        result['frontends'] = caps
    return result


def detect_satip_devices():
    """ Detect available SAT>IP devices by sending a broadcast message.

    Returns:
        list: Found SAT>IP devices.
    """
    urls = []
    devices = []
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(0)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except BaseException:
        pass
    sock.settimeout(1)
    sock.bind((SSDP_BIND, SSDP_PORT))
    sock.sendto(SSDP_REQUEST, (SSDP_ADDR, SSDP_PORT))
    try:
        while 1:
            data = sock.recv(1024)
            if data:
                for row in data.split('\r\n'):
                    if 'LOCATION:' in row:
                        url = row.replace('LOCATION:', '').strip()
                        if url in urls:
                            continue
                        urls.append(url)
                        info = requests.get(url, timeout=2)
                        devices.append(parse_satip_xml(info.text))
            else:
                break
    except BaseException:
        pass
    sock.close()
    return devices


if __name__ == '__main__':
    json.dump(detect_satip_devices(), fp=sys.stdout, sort_keys=True, indent=2)
