# Basilisk-IaC

IN PROGRESS!

Last verified versions:
- Ansible [core 2.19.10]

1. Set up infrastructure
`c
$ source .env #API keys and other stuff
$ tofu plan
$ tofu create ...
`

#) Run playbooks:
`$ ansible-playbook -i inventory/inventory.ini playbooks/<playbook>.yaml --ask-vault-password`
