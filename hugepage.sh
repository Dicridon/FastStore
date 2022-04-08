#!/bin/bash
echo 4096 > /proc/sys/vm/nr_hugepages
grep Hugepage /proc/meminfo -i
