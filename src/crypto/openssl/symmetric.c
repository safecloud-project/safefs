#include "symmetric.h"

int KEYSIZE;
unsigned char* KEY;

void handleErrors(void)
{
  ERR_print_errors_fp(stderr);
  abort();
}


unsigned char* openssl_rand_str(int length) {    
    static int mySeed = 25011984;
    char *string = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
    size_t stringLen = strlen(string);        
    unsigned char *randomString = NULL;

    srand(time(NULL) * length + ++mySeed);

    if (length < 1) {
        length = 1;
    }

    randomString = malloc(sizeof(char) * (length +1));

    if (randomString) {
        short key = 0;
	    int n;
        for (n = 0;n < length;n++) {
            key = rand() % stringLen;          
            randomString[n] = string[key];
        }

        randomString[length] = '\0';

        return randomString;        
    }
    else {
        ERROR_MSG("No memory");
        exit(1);
    }
}


int openssl_init(char* key, int local_key_size){
  if (key == NULL)
  {
    //ERROR_MSG("(symmetric.c) - init's key argument is NULL");
    exit(1);
  }

  ERR_load_crypto_strings();
  OpenSSL_add_all_algorithms();
  OPENSSL_config(NULL);
  KEYSIZE = local_key_size;
  KEY = (unsigned char*) key;
  return 0;
}

const EVP_CIPHER* get_cipher(){

    switch(KEYSIZE){
        case 16:
            return EVP_aes_128_cbc();
        case 24:
            return EVP_aes_192_cbc();
        default:
            return EVP_aes_256_cbc();
    }
}


int openssl_encode(unsigned char* iv, unsigned char* dest, const unsigned char* src, int size){
    
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;

    /* Create and initialize the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
        handleErrors();

      /* Initialise the encryption operation. IMPORTANT - ensure you use a key
       * and IV size appropriate for your cipher
       * In this example we are using 256 bit AES (i.e. a 256 bit key). The
       * IV size for *most* modes is the same as the block size. For AES this
       * is 128 bits */
    if(1 != EVP_EncryptInit_ex(ctx, get_cipher(), NULL, KEY, iv))
        handleErrors();

    if(1 != EVP_EncryptUpdate(ctx, dest, &len, src, size))
        handleErrors();

      /* Finalize the encryption. Further ciphertext bytes may be written at
       * this stage.
       */
    ciphertext_len = len;
    if(1 != EVP_EncryptFinal_ex(ctx, dest + len, &len)) 
        handleErrors();
    
    ciphertext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext_len;

}

int openssl_decode(unsigned char *iv, unsigned char* dest, const unsigned char* src, int size)
{

    EVP_CIPHER_CTX *ctx;
    
    int len;
    int plaintext_len;
    
    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) 
        handleErrors();

  /* Initialize the decryption operation. IMPORTANT - ensure you use a key
   * and IV size appropriate for your cipher
   * In this example we are using 256 bit AES (i.e. a 256 bit key). The
   * IV size for *most* modes is the same as the block size. For AES this
   * is 128 bits */

    if(1 != EVP_DecryptInit_ex(ctx, get_cipher(), NULL, KEY, iv))
        handleErrors();

      /* Provide the message to be decrypted, and obtain the plaintext output.
       * EVP_DecryptUpdate can be called multiple times if necessary
       */
    

    if(1 != EVP_DecryptUpdate(ctx, dest, &len, src, size))
        handleErrors();
    
    plaintext_len = len;
      /* Finalise the decryption. Further plaintext bytes may be written at
       * this stage.
       */
    if(1 != EVP_DecryptFinal_ex(ctx, dest + len, &len)) 
        handleErrors();
    plaintext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);
    
    return plaintext_len;
}

int openssl_clean(){
	 return 0;
}


