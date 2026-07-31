#!/usr/bin/env python3
from stem.control import Controller

controller = Controller.from_port(port=9051)
controller.authenticate()  # Use cookie or auth automatically


def get_bw_utilized(controller):
    bytes_read = int(controller.get_info("traffic/read"))
    bytes_written = int(controller.get_info("traffic/written"))

    # Convert to GB
    gb_read = round(bytes_read / (1024**3), 2)
    gb_written = round(bytes_written / (1024**3), 2)
    bandwidth = [gb_read, gb_written]
    return bandwidth

def get_conn(controller):
    # Total active connections (circuits/OR connections)
    connections = len(controller.get_circuits())
    return connections

def get_uptime(controller):
    # Get uptime in seconds
    uptime = controller.get_info("uptime")
    return uptime

if __name__ == "__main__":
    statistics = {}
    bw_used = get_bw_utilized(controller)
    total_conn = get_conn(controller)
    uptime = get_uptime(controller)
    statistics["BW_used"] = bw_used
    print(statistics)

#print(f"Total Received: {gb_read} GB")
#print(f"Total Sent:     {gb_written} GB")
#print(f"Active Circuits: {connections}")
#print(f"Uptime:         {uptime} seconds")
