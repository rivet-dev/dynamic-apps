// This is a sample Terraform file
/*
Block comment
*/
# shell like comment

variable enabled {
  default                           = "this is a variable"
  type                              = string
}

locals {
  list                           = [
    "this",
    "is",
    "a",
    "list",
  ]
}

data data_type label {
  id                              = 1
}

resource resource_type resource_name {
  for_each                          = var.enabled == false ? [] : toset([for item in var.list:
    "this is the item ${item}" if item != ''
  ])
  description                       =<<-EOF
this is a heredoc
EOF

  dynamic test {

  }
}
