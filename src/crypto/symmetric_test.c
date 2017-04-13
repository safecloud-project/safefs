#include <stdio.h>
#include <gcrypt.h>
#include "det_symmetric.h"

int main(int argc, char **argv) {
    /* A 256 bit key */
    unsigned char *key = (unsigned char *)"01234567890123456789012345678901";

    /* A 128 bit IV */
    unsigned char *iv = (unsigned char *)"01234567890123456";

    det_init(key, iv, 32);
    printf("Init working\n");
    char plain_text[4096];
    int i;
    for (i = 0; i < 4096; i++) {
        plain_text[i] = 'a';
    }
    int plain_text_size = strlen(plain_text);
    printf("size of string %d\n", plain_text_size);
    unsigned char ciphertext[plain_text_size];
    unsigned char decryptedtext[plain_text_size];

    // int out_size = sizeof(char)*plain_text_size;
    printf("Going to decode\n");
    plain_text_size = det_encode((unsigned char *)ciphertext, (const unsigned char *)plain_text, plain_text_size, "");
    printf("size of encoded string is %d\n", (int)strlen(ciphertext));
    printf("plain text size after encoding %d\n", plain_text_size);

    printf("Encoded message is %s\n", ciphertext);
    printf("decyphered message is %s\n", decryptedtext);

    det_decode((unsigned char *)decryptedtext, (const unsigned char *)ciphertext, plain_text_size, "");
    printf("Encoded message is %s\n", decryptedtext);
    det_clean();
}
