#!/usr/bin/python3

import select
import time
import requests
from requests.exceptions import RequestException
from os import getenv

api_push_key = getenv("api_key")
CHECK_INTERVAL = 30  # seconds

# Color codes for alerts
code_red = 15158332
code_green = 446839
code_orange = 16741120

def send_discord_alert(title, description, code):
    if not api_push_key:
        raise ValueError("API_PUSH_KEY environment variable is not set.")

    # Construct the payload safely
    payload = {
        "embeds": [
            {
                "title": title,
                "description": description,
                "color": code
            }
        ]
    }

    try:
        # 2. Timeout (connect timeout, read timeout) to prevent hanging
        # 3. 'json=payload' instead of manual string formatting to prevent malformed bodies
        response = requests.post(
            "https://<<API URL>>" + api_push_key,
            json=payload,
            timeout=(3.0, 10.0)
        )

        # 4. Handle Rate Limits (HTTP 429) gracefully
        if response.status_code == 429:
            retry_after = response.json().get("retry_after", 1)
            print(f"Rate limited by Discord. Retry after {retry_after} seconds.")
            return False

        # Raise an exception for other bad status codes (4xx, 5xx)
        response.raise_for_status()

        print("Alert sent successfully.")
        return True

    except RequestException as e:
        print(f"Network or HTTP error occurred while sending webhook: {e}")
        return False

def check_memory_psi():
    try:
        with open("/proc/pressure/memory", "r") as f:
            line = f.readline()
            if "some" in line:
                parts = line.split()
                # Extract 10-second average pressure metric
                avg10 = float(parts[1].split("=")[1])
                return avg10
    except Exception as e:
        print(f"Error reading PSI: {e}")
    return 0.0

def main():
    print("Starting low-resource Tor memory monitor (0% CPU idle)...")
    send_discord_alert("Starting Test", "Test desc", code_green)
    while True:
        # Read current memory pressure 10-second average
        pressure = check_memory_psi()

        # If pressure 10s average exceeds 5.0 (meaning tasks are stalled 5% of the time)
        if pressure > 1.0:
            send_discord_alert(
                "Memory Pressure High!",
                f"Kernel reports memory pressure 10s avg is {pressure}%"
            )
            time.sleep(120)  # Cooldown after alerting

        # Sleep natively using select timeout (consumes 0 CPU cycles)
        select.select([], [], [], CHECK_INTERVAL)

if __name__ == "__main__":
    main()
