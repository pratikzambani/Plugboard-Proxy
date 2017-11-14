pbproxy:
	gcc pbproxy.c -lcrypto -o pbproxy

clean:
	rm -f *.o *.out pbproxy
