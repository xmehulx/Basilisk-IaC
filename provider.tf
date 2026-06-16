terraform {
  required_providers {
    proxmox = {
      source = "bpg/proxmox"
    }
  }
}

provider "proxmox" {
  endpoint = "https://10.0.0.236:8006/api2/json"
  api_token = "terraform@pve!basilisk=9254bd84-117e-4e4f-bdb1-80d1ef8f44cd"
  insecure = true
}
