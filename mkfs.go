package main

import (
	"os"
	"fmt"
	"bytes"
	"time"
	"encoding/binary"
)

func init_super_block(f *os.File) (err error) {
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.LittleEndian, uint32(2))
	binary.Write(buf, binary.LittleEndian, uint32(8192))
	binary.Write(buf, binary.LittleEndian, uint32(58200))
	binary.Write(buf, binary.LittleEndian, uint32(2))
	binary.Write(buf, binary.LittleEndian, uint32(8192))
	binary.Write(buf, binary.LittleEndian, uint32(8192))
	f.Write(buf.Bytes())
	return
}

func init_inode_table(f *os.File) (err error) {
	f.Seek(512, 0)
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.LittleEndian, uint32(1))
	f.Write(buf.Bytes())
	buf.Reset()

	for i := 0; i < 511; i++ {
		binary.Write(buf, binary.LittleEndian, uint32(0))
	}
	f.Write(buf.Bytes())
	buf.Reset()

	f.Seek(512 * 5, 0)
	binary.Write(buf, binary.LittleEndian, uint32(0))
	binary.Write(buf, binary.LittleEndian, uint32(time.Now().UnixNano()))
	binary.Write(buf, binary.LittleEndian, uint32(16384))
	binary.Write(buf, binary.LittleEndian, uint32(2))
	for i := 0; i < 15; i++ {
	    binary.Write(buf, binary.LittleEndian, uint32(0))
	}
	f.Write(buf.Bytes())
	return
}


func main() {
	if len(os.Args) != 2 {
		fmt.Println("Usage: mkfs dev_name")
		return
	}
	dev_name := os.Args[1]
	f, err := os.OpenFile(dev_name, os.O_WRONLY, 0644)
	if err != nil {
		fmt.Println(err)
		return
	}
	defer f.Close()
	init_super_block(f)
	init_inode_table(f)
	return
}
