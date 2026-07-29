CC = arm-linux-gnueabihf-gcc

cameraAPP:cameraAPP.o
	$(CC) $< -o $@ -static

cameraAPP.o:cameraAPP.c
	$(CC) -c $<

clean:
	rm -rf cameraAPP *.o