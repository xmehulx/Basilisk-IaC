terraform {
  required_providers {
    proxmox = {
      source = "bpg/proxmox"
    }
  }
}

provider "proxmox" {
  endpoint = "https://10.0.0.236:8006/api2/json"
  api_token = var.terra-key 
  insecure = true
}
