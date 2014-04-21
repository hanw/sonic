#!/bin/bash

#00:03.0 is Root Port address
# Need to enable 2 virtual channels on Root Port.
#sudo setpci -v -GD -s 00:03.0 117.b=80:80
#sudo setpci -v -GD -s 00:03.0 123.b=80:80

#03:00.0 is PCIe Endpoint address
# VC0
sudo setpci -v -G -s 03:00.0 114.b=FF:FF
sudo setpci -v -G -s 03:00.0 117.b=80:80
# VC1
#sudo setpci -v -G -s 03:00.0 120.b=F0:FF
#sudo setpci -v -G -s 03:00.0 123.b=80:80
