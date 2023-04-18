all: redextract

redextract: packet.c packet.h pcap-read.c pcap-read.h pcap-process.c pcap-process.h main.c spooky.h spooky.c
	gcc packet.c pcap-process.c pcap-read.c main.c spooky.c -Wall --std=c99 -lpthread -lm -o redextract