#! /usr/bin/python

import sys
import subprocess

pset_num = raw_input("Enter the PaRAM Set Number: ");
int(pset_num)

pset_addr = hex(0x4000 + 20 * 2**5);
print "0x4900" + pset_addr
subprocess.call(["sudo", "devmem2", "0x49004280"]);
