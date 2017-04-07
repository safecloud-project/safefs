CC = gcc

teste: symmetric_test.c symmetric_det.o
	$(CC) $< `libgcrypt-config --cflags --libs` -o $@

symmetric_det.o: symmetric.c det_symmetric.c
	$(CC) -c  $< `libgcrypt-config --cflags` -o $@

symmetric_det_openssl.o: det_symmetric.c openssl/symmetric.c
	$(CC) -c $< -o $@ 

teste_openssl: symmetric_test.c det_symmetric.c openssl/symmetric.c
	$(CC)  symmetric_test.c det_symmetric.c openssl/symmetric.c -o $@ -lssl -lcrypto

clean:
	rm -f $(TARGETS) *.o test

