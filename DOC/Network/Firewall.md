# Rules

## Proxy-Basilisk
1. ALLOW in TCP:HTTP/S              // Move to only HTTPS
2. ALLOW in TCP:SSH from Infra
3. DENY out *

## Tor-Basilisk
1. DROP out * to 10.0.0.0/8
2. ALLOW in TCP:SSH from Infra
3. ACCEPT in TCP:443 from *
