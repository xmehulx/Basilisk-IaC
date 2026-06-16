resource "proxmox_virtual_environment_container" "proxy-basilisk-tf" {
  node_name = "pve"
  vm_id = 104
  
  cpu {
    cores = 2
  }

  memory {
    dedicated = 1024
  }

  disk {
    datastore_id = "local-lvm"
    size         = 7
  }

  operating_system {
    template_file_id = "local:vztmpl/debian-12-standard_12.12-1_amd64.tar.zst"
  }

  initialization {
    hostname = "searxng-basilisk"
    ip_config {
      ipv4 {
        address = "10.0.0.126"
        gateway = "10.0.0.1"
      }
    }

    dns {
      servers = ["10.0.0.124"]
    }
  }
}
