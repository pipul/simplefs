#!/bin/bash

blocksize=512

showinode() {
	sb=$((2560 + $1 * 76))
	hexdump -d -s $sb -n 76 /dev/mmcblk0p1
}

showde() {
	sb=$((4196864 + $1 * 28))
	hexdump -d -s $sb -n 28 /dev/mmcblk0p1
}


if [[ $1 == inode ]]; then
	showinode $2
elif [[ $1 == de ]]; then
	showde $2
fi
