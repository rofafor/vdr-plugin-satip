#!/usr/bin/env python2

import json
import socket
import sys
import urllib2
import xml.etree.ElementTree as ET

SSDP_ADDR = "239.255.255.250";
SSDP_PORT = 1900;
SSDP_MX = 1;
SSDP_ST = "urn:ses-com:device:SatIPServer:1";

ssdpRequest = "M-SEARCH * HTTP/1.1\r\n" + \
                "HOST: %s:%d\r\n" % (SSDP_ADDR, SSDP_PORT) + \
                "MAN: \"ssdp:discover\"\r\n" + \
                "MX: %d\r\n" % (SSDP_MX, ) + \
                "ST: %s\r\n" % (SSDP_ST, ) + "\r\n";

data = None
urls = list()
devices = list()

# find out available SAT>IP servers
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setblocking(0)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
except:
    pass
sock.settimeout(1)
sock.bind(("192.168.0.6", SSDP_PORT))
sock.sendto(ssdpRequest, (SSDP_ADDR, SSDP_PORT))
try:
    while 1:
        data = sock.recv(1024)
        if data:
            # find out their capabilities
            for row in data.split("\r\n"):
                if "LOCATION:" in row:
                    url = row.replace("LOCATION:","").strip()
                    if url in urls:
                        continue
                    urls.append(url)
                    f = urllib2.urlopen(url, timeout=2)
                    xml = f.read()
                    if xml:
                        root = ET.fromstring(xml)
                        name = root.find(".//*/{urn:schemas-upnp-org:device-1-0}friendlyName")
                        satipcap = root.find(".//*/{urn:ses-com:satip}X_SATIPCAP")
                        caps = dict()
                        for systems in satipcap.text.split(","):
                            cap = systems.split("-")
                            if cap:
                                # prepare the output data
                                count = int(cap[1])
                                if cap[0] in caps:
                                    count = count + caps[cap[0]]
                                caps[cap[0]] = count
                        devices.append({"name": name.text, "frontends": caps})
        else:
            break
except:
    pass
sock.close()

# print out the output data
json.dump(devices, fp=sys.stdout, sort_keys=True, indent=2)
