resource "proxmox_virtual_environment_container" "adguard-basilisk" {
  node_name = "pve"
  vm_id = 102

  unprivileged = true

  features {
    nesting = true
  }
  
  cpu {
    cores = 1
  }

  memory {
    dedicated = 256
    swap = 128
  }

  disk {
    datastore_id = "local-lvm"
    size         = 4
  }

  operating_system {
    template_file_id = "local:vztmpl/debian-12-standard_12.12-1_amd64.tar.zst"
    type = "debian"
  }

  initialization {
    hostname = "adguard-basilisk"

    user_account {
      password = var.root_password
      keys = [
        file("/home/infra/basilisk-iac/.ssh/root-basilisk.pub")
      ]
    }
    
    ip_config {
      ipv4 {
        address = "10.0.0.124/24"
        gateway = "10.0.0.1"
      }
    }
  }
  
  network_interface {
    name = "eth0"
    bridge = "vmbr0"
    firewall = true
  }
}
