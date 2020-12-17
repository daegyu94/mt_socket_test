obj-m += network_client_mt.o 
#obj-m += network_server.o 

all:
	make -j 16 -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -j 16 -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load_server:
	insmod ./network_server.ko

load_client:
	insmod ./network_client_mt.ko

unload_server:
	rmmod ./network_server.ko

unload_client:
	rmmod ./network_client_mt.ko


