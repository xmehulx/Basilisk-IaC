resource "proxmox_virtual_environment_container" "tor-tf" {
  node_name = "pve"
  vm_id = 111

  unprivileged = true

  features {
    nesting = true
  }
  
  cpu {
    cores = 2 
  }

  memory {
    dedicated = 1024 
  }

  disk {
    datastore_id = "local-lvm"
    size         = 2
  }

  operating_system {
    template_file_id = "local:vztmpl/debian-12-standard_12.12-1_amd64.tar.zst"
    type = "debian"
  }

  initialization {
    hostname = "tor-basilisk"

    user_account {
      password = var.root_password
      keys = [
        file("/home/infra/.ssh/root-basilisk.pub"),
        file("/home/infra/.ssh/ansible_basilisk.pub")
      ]
    }
    
    ip_config {
      ipv4 {
        address = "10.0.0.2/24"
        gateway = "10.0.0.1"
      }
    }

    dns {
      servers = ["1.1.1.1", "9.9.9.9"]
    }
  }
  
  network_interface {
    name = "eth0"
    bridge = "vmbr0"
    firewall = true
  }
}
