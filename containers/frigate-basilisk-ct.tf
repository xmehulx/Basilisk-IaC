resource "proxmox_virtual_environment_container" "frigate-basilisk" {
  node_name = "pve"
  vm_id = 107 

  unprivileged = true

  features {
    nesting = true
  }
  
  cpu {
    cores = 4
  }

  memory {
    dedicated = 4096
    swap = 1024
  }

  disk {
    datastore_id = "local-lvm"
    size         = 20
  }

  mount_point {
    volume = "/mnt/nvme/frigate"
    path   = "/media/frigate"
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
        address = "10.0.0.122/24"
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
