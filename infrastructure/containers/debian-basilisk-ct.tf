resource "proxmox_virtual_environment_container" "debian-basilisk" {
  node_name = "pve"
  vm_id = 100 

  unprivileged = true

  features {
    nesting = true
  }
  
  cpu {
    cores = 4
  }

  memory {
    dedicated = 4096
  }

  disk {
    datastore_id = "local-lvm"
    size         = 50
  }

  mount_point {
    volume = "/mnt/nvme/photo_bin"
    path   = "/photos"
  }

  operating_system {
    template_file_id = "local:vztmpl/debian-12-standard_12.12-1_amd64.tar.zst"
    type = "debian"
  }

  initialization {
    hostname = "debian-basilisk"

    user_account {
      password = var.root_password
      keys = [
        file("/home/infra/basilisk-iac/.ssh/root-basilisk.pub")
      ]
    }
    
    ip_config {
      ipv4 {
        address = "10.0.0.30/24"
        gateway = "10.0.0.1"
      }
    }

    dns {
      servers = ["10.0.0.124"]
    }
  }
  
  network_interface {
    name = "eth0"
    bridge = "vmbr0"
    firewall = true
  }
}
