# Rules

## 100. Debian-Basilisk
1. Allow in SSH from Infra
2. Allow in TCP     :445 (SMB)
3. Allow in UDP     :1900 for HA UPNP (Optional)
4. Allow in TCP     :2283 for Immich
5. Allow in UDP     :5353 for HA mDNS Discovery
6. Allow in TCP     :8123 for HA WebUI

## 101. Wireguard-Basilisk
1. Allow in SSH from Infra
2. Allow in UDP     :51820 for Wireguard Listener

## 102. Adguard-Basilisk
1. Allow in TCP/UPD:DNS
2. Allow in TCP     :80 for WebUI
3. Allow in TCP     :853 for DoT
4. Allow in TCP     :3000 for initial setup (temporary)

## 103. Vaultwarden-Basilisk
1. Allow in SSH from Infra
2. Allow in TCP     :8080 for WebUI

## 104. SearXNG-Basilisk
1. Allow in SSH from Infra
2. Allow in HTTP/S

## 105. Proxy-Basilisk
1. ALLOW in TCP:HTTP/S              // Move to only HTTPS
2. ALLOW in TCP:SSH from Infra
3. DENY out *

## 106. Infra-Basilisk
1. Allow in SSH from my PCs

## 107. Frigate-Basilisk
1. Allow in SSH from Infra
2. Allow in TCP     :5000
3. Allow in TCP/UDP :8554
4. Allow in TCP/UDP :8555

## 11. Tor-Basilisk
1. DROP out * to 10.0.0.0/8
2. ALLOW in SSH from Infra
3. ACCEPT in TCP    :443 from *
