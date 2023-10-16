
setup_vlan()
{
name=$1
vlanid=$2
ip=$3
mac=$4
iface=`ifconfig |grep enx | awk -F ":" '{print $1}'`

if [ x${iface} = x"" ];then
  echo not found usb ethernet
else
  echo found usb ethernet $iface
  ip link add link ${iface} name ${name}.${vlanid} type vlan id ${vlanid}
  ip addr add ${ip} dev ${name}.${vlanid}
  ifconfig ${name}.${vlanid} hw ether ${mac} 
  ip link set up ${name}.${vlanid}
fi
}
setup_arp()
{
ip=$1
mac=$2
iface=`ifconfig |grep enx | awk -F ":" '{print $1}'`
if [ x${iface} = x"" ];then
  echo not found usb ethernet
else
  echo found usb ethernet ${iface}
  arp -s ${ip} ${mac}
fi
}

main()
{
# #ADPU simulation
# setup_vlan ADPU 204 10.204.0.1/16 5A:45:45:00:00:0F
# setup_vlan ADPU 205 10.205.0.1/16 5A:45:45:00:00:0F
# setup_vlan ADPU 206 10.206.0.1/16 5A:45:45:00:00:0F

# #CSCBCACore simulation
# setup_vlan CSCBCACore 201 10.201.0.1/16 5A:45:45:00:00:01
# setup_vlan CSCBCACore 204 10.204.0.1/16 5A:45:45:00:00:01
# setup_vlan CSCBCACore 205 10.205.0.1/16 5A:45:45:00:00:01
# setup_vlan CSCBCACore 206 10.206.0.1/16 5A:45:45:00:00:01
# setup_vlan CSCBCACore 207 10.207.0.1/16 5A:45:45:00:00:01
# setup_vlan CSCBCACore 208 10.208.0.1/16 5A:45:45:00:00:01

#CSCBCMCore simulation
#setup_vlan CSCBCMCore 201 10.201.0.2/16 5A:45:45:00:00:02
#setup_vlan CSCBCMCore 204 10.204.0.2/16 5A:45:45:00:00:02
#setup_vlan CSCBCMCore 205 10.205.0.2/16 5A:45:45:00:00:02
#setup_vlan CSCBCMCore 206 10.206.0.2/16 5A:45:45:00:00:02
#setup_vlan CSCBCMCore 207 10.207.0.2/16 5A:45:45:00:00:02
#setup_vlan CSCBCMCore 208 10.208.0.2/16 5A:45:45:00:00:02

#ZCL simulation
setup_vlan ZCL 205 10.205.0.9/16 5A:45:45:00:00:09
setup_vlan ZCL 208 10.208.0.9/16 5A:45:45:00:00:09

#arp table for ZCT
setup_arp 10.201.0.19 5A:45:45:00:00:13
setup_arp 10.204.0.19 5A:45:45:00:00:13
setup_arp 10.205.0.19 5A:45:45:00:00:13
setup_arp 10.206.0.19 5A:45:45:00:00:13
setup_arp 10.208.0.19 5A:45:45:00:00:13
}

main
