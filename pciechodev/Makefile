obj-m += echodev-drv.o

all:
	make -C ../linux-6.1.135 M=$(PWD) modules

clean:	
	make -C ../linux-6.1.135 M=$(PWD) clean
