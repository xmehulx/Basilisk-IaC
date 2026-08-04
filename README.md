# Basilisk-IaC

IN PROGRESS!

A home lab with the following services:
1. Caddy
2. Tor Relay
3. Immich
4. Wireguard
5. SMB server
6. Bitwarden
7. SearXNG

Last verified versions:
- Ansible [core 2.19.10]


# TO DO!!!!
Frigate recording
SMB share

# Firewall:
## tor-basilisk
- ALLOW TCP:22 from infra to tor
- ALLOW TCP:443 IN
- DENY OUT to 10.0.0.0/8

# 1. Setting up Basilisk
## 1.1 Set up infrastructure
```c
$ source .env #API keys and other stuff
$ tofu plan
$ tofu create ...
```

#) Run playbooks:
```
$ ansible-playbook -i inventory/inventory.ini playbooks/<playbook>.yaml --ask-vault-password
```

## 1.2 Set up Containers
### 1.2.1 Wireguard
#### 1.2.1.4 Adding new clients
```
# wg genkey | tee client_private.key | wg pubkey > client_public.key
# wg genpsk > client_psk.key
```
Edit your wireguard .conf file and add the following:
```
[Peer]
PublicKey = <<Public Key>>
PresharedKey = <<Pre-Shared Key>>
AllowedIPs = 10.0.10.1/32                   # Put client IP here
```
And restart SMB Daemon:
```
# wg syncconf wg0 <(wg-quick strip wg0)
```

Create client config file:
- Split Tunnel
```
[Interface]
PrivateKey = <<Private Key>>
Address = 10.0.10.2/32
DNS = <<DNS Servers>>

[Peer]
PublicKey = <<Public Key>>
PresharedKey = <<Pre-Shared Key>>
Endpoint = <<Public IP>>:51820              # Forward this port on the router
AllowedIPs = 192.168.1.0/24, 192.168.2.0/24
PersistentKeepalive = 25 
```

# Services
## Tor
Custom Tor service which keeps track of your system resources and sends alerts to your email/teams/phone
