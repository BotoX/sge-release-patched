/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <unistd.h>
#include <stdio.h>

#ifdef SECURE
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#include "uti/sge_arch.h"
#include "uti/sge_log.h"
#include "uti/sge_unistd.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_profiling.h"
#include "uti/sge_bootstrap.h"
#include "uti/setup_path.h"
#include "uti/sge_prog.h"
#include "uti/sge_stdio.h"
#if defined(DEFINE_SGE_PASSWD_MAIN)
#include "sgermon.h"
#endif

#include "sge_passwd.h"
#include "msg_utilbin.h"

#if !defined(DEFINE_SGE_PASSWD_MAIN)
#define DENTER(x,y)
#define DPRINTF(x)
#define DEXIT
#endif

static void (*shared_ssl_func__X509_free)(X509 *a);
static void (*shared_ssl_func__EVP_PKEY_free)(EVP_PKEY *pkey);
static void (*shared_ssl_func__ERR_print_errors_fp)(FILE *fp);
static int (*shared_ssl_func__EVP_PKEY_size)(EVP_PKEY *pkey);
static const EVP_CIPHER* (*shared_ssl_func__EVP_rc4)(void);
static int (*shared_ssl_func__EVP_OpenInit)(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *type, unsigned char *ek, int ekl, unsigned char *iv, EVP_PKEY *priv);
static int (*shared_ssl_func__EVP_DecryptUpdate)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl);
static int (*shared_ssl_func__EVP_EncryptUpdate)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in, int inl);
static int (*shared_ssl_func__EVP_SealFinal)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl);
static int (*shared_ssl_func__EVP_SealInit)(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *type, unsigned char **ek, int *ekl, unsigned char *iv, EVP_PKEY **pubk, int npubk);
static int (*shared_ssl_func__EVP_OpenFinal)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl);
static int (*shared_ssl_func__EVP_read_pw_string)(char *buf, int length, const char *prompt, int verify);
static void (*shared_ssl_func__ERR_load_crypto_strings)(void);
static EVP_PKEY* (*shared_ssl_func__X509_get_pubkey)(X509 *x);
static char* (*shared_ssl_func__PEM_ASN1_read)(char *(*d2i)(),const char *name,FILE *fp,char **x, pem_password_cb *cb, void *u);
EVP_PKEY* (*shared_ssl_func__d2i_AutoPrivateKey)(EVP_PKEY **a, unsigned char **pp, long length);
static X509* (*shared_ssl_func__d2i_X509)(X509 **a, unsigned char **in, long len);

#define shared_ssl_func__EVP_OpenUpdate(a,b,c,d,e) shared_ssl_func__EVP_DecryptUpdate(a,b,c,d,e)
#define shared_ssl_func__EVP_SealUpdate(a,b,c,d,e) shared_ssl_func__EVP_EncryptUpdate(a,b,c,d,e)
#define shared_ssl_func__X509_extract_key(x) shared_ssl_func__X509_get_pubkey(x)
#define shared_ssl_func__PEM_read_X509(fp,x,cb,u) (X509 *)shared_ssl_func__PEM_ASN1_read((char *(*)())shared_ssl_func__d2i_X509,PEM_STRING_X509,fp,(char **)x,cb,u)
#define shared_ssl_func__PEM_read_PrivateKey(fp,x,cb,u) (EVP_PKEY *)shared_ssl_func__PEM_ASN1_read((char *(*)())shared_ssl_func__d2i_AutoPrivateKey,PEM_STRING_EVP_PKEY,fp,(char **)x,cb,u)

#ifdef LOAD_OPENSSL
static void* shared_ssl_lib = NULL;
#endif

int
sge_init_shared_ssl_lib(void) 
{
   int ret;

   DENTER(TOP_LAYER, "sge_init_shared_ssl_lib");
#ifdef LOAD_OPENSSL  
   if (shared_ssl_lib == NULL) {
#  if defined(DARWIN)
#     ifdef RTLD_NODELETE
      shared_ssl_lib = dlopen ("libcrypto.bundle", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
#     else
      shared_ssl_lib = dlopen ("libcrypto.bundle", RTLD_NOW | RTLD_GLOBAL );
#     endif /* RTLD_NODELETE */
#  elif defined(HP11)
#     ifdef RTLD_NODELETE
      shared_ssl_lib = dlopen ("libcrypto.sl", RTLD_LAZY | RTLD_NODELETE);
#     else
      shared_ssl_lib = dlopen ("libcrypto.sl", RTLD_LAZY );
#     endif /* RTLD_NODELETE */
#  else
#     ifdef RTLD_NODELETE
      shared_ssl_lib = dlopen ("libcrypto.so", RTLD_LAZY | RTLD_NODELETE);
#     else
      shared_ssl_lib = dlopen ("libcrypto.so", RTLD_LAZY);
#     endif /* RTLD_NODELETE */
#endif

      if (shared_ssl_lib != NULL) {
         const char *func_name[] = {
            "X509_free",
            "EVP_PKEY_free",
            "ERR_print_errors_fp",
            "EVP_PKEY_size",  
            "EVP_rc4",
   
            "EVP_OpenInit", 
            "EVP_DecryptUpdate", 
            "EVP_EncryptUpdate", 
            "EVP_SealFinal", 
            "EVP_SealInit",

            "EVP_OpenFinal",
            "EVP_read_pw_string",
            "ERR_load_crypto_strings",
            "X509_get_pubkey",
            "PEM_ASN1_read",
            "d2i_AutoPrivateKey",
            "d2i_X509",
            NULL
         };

         const void *func_ptr[] = {
            &shared_ssl_func__X509_free,
            &shared_ssl_func__EVP_PKEY_free,
            &shared_ssl_func__ERR_print_errors_fp,
            &shared_ssl_func__EVP_PKEY_size,
            &shared_ssl_func__EVP_rc4,

            &shared_ssl_func__EVP_OpenInit,
            &shared_ssl_func__EVP_DecryptUpdate,
            &shared_ssl_func__EVP_EncryptUpdate,
            &shared_ssl_func__EVP_SealFinal,
            &shared_ssl_func__EVP_SealInit,

            &shared_ssl_func__EVP_OpenFinal,
            &shared_ssl_func__EVP_read_pw_string,
            &shared_ssl_func__ERR_load_crypto_strings,
            &shared_ssl_func__X509_get_pubkey,
            &shared_ssl_func__PEM_ASN1_read,

            &shared_ssl_func__d2i_AutoPrivateKey,
            &shared_ssl_func__d2i_X509,

            NULL
         };
         int i = 0;

         while (func_name[i] != NULL) {
            *((int**)(func_ptr[i])) = (int*)dlsym(shared_ssl_lib, func_name[i]);

            if (*((int**)(func_ptr[i])) == NULL) {
               fprintf(stderr, "%s: unable to initialize function %s\n", 
                       prognames[SGE_PASSWD], func_name[i]);            
               exit(1);
            } else {
               DPRINTF(("function "SFQ" successfully initialized\n",
                        func_name[i]));
            }
            i++;
         }
        
         ret = 0;
      } else {
         fprintf(stderr, MSG_PWD_CANT_OPEN_SSL_LIB, prognames[SGE_PASSWD]);
         ret = 1;
      }
   } else {
      ret = 1;
   }
#else
   shared_ssl_func__X509_free = X509_free,
   shared_ssl_func__EVP_PKEY_free = EVP_PKEY_free,
   shared_ssl_func__ERR_print_errors_fp = ERR_print_errors_fp,
   shared_ssl_func__EVP_PKEY_size = EVP_PKEY_size,
   shared_ssl_func__EVP_rc4 = EVP_rc4,

   shared_ssl_func__EVP_OpenInit = EVP_OpenInit,
   shared_ssl_func__EVP_DecryptUpdate = EVP_DecryptUpdate,
   shared_ssl_func__EVP_EncryptUpdate = EVP_EncryptUpdate,
   shared_ssl_func__EVP_SealFinal = EVP_SealFinal,
   shared_ssl_func__EVP_SealInit = EVP_SealInit,

   shared_ssl_func__EVP_OpenFinal = EVP_OpenFinal,
   shared_ssl_func__EVP_read_pw_string = EVP_read_pw_string,
   shared_ssl_func__ERR_load_crypto_strings = ERR_load_crypto_strings,
   shared_ssl_func__X509_get_pubkey = X509_get_pubkey,
   shared_ssl_func__PEM_ASN1_read = PEM_ASN1_read,

   shared_ssl_func__d2i_AutoPrivateKey = d2i_AutoPrivateKey,
   shared_ssl_func__d2i_X509 = d2i_X509,
   ret = 0;
#endif
   DEXIT;
   return ret;
}

int
sge_done_shared_ssl_lib(void)
{
   int ret;

   DENTER(TOP_LAYER, "sge_done_shared_ssl_lib");
#ifdef LOAD_OPENSSL
   if (shared_ssl_lib != NULL) {
      shared_ssl_func__X509_free = NULL;
      shared_ssl_func__EVP_PKEY_free = NULL;
      shared_ssl_func__ERR_print_errors_fp = NULL;
      shared_ssl_func__EVP_PKEY_size = NULL;
      shared_ssl_func__EVP_rc4 = NULL;
      shared_ssl_func__EVP_OpenInit = NULL;
      shared_ssl_func__EVP_DecryptUpdate = NULL;
      shared_ssl_func__EVP_EncryptUpdate = NULL;
      shared_ssl_func__EVP_SealFinal = NULL;
      shared_ssl_func__EVP_SealInit = NULL;
      shared_ssl_func__EVP_OpenFinal = NULL;
      shared_ssl_func__EVP_read_pw_string = NULL;
      shared_ssl_func__ERR_load_crypto_strings = NULL;
      shared_ssl_func__X509_get_pubkey = NULL;
      shared_ssl_func__PEM_ASN1_read = NULL;
      shared_ssl_func__d2i_AutoPrivateKey = NULL;
      shared_ssl_func__d2i_X509 = NULL;
      dlclose(shared_ssl_lib);
      shared_ssl_lib = NULL;
      ret = 0;
   } else {
      ret = 1;
   }
#else
   ret = 0;
#endif
   DEXIT;
   return ret;
}

static const char*
sge_get_file_pub_key(void)
{
   static char file[4096] = "";

   DENTER(TOP_LAYER, "sge_get_file_pub_key");
   if (file[0] == '\0') {
      const char *cert = getenv("SGE_CERTFILE");

      if (cert != NULL) {
         sprintf(file, cert);
      } else {
         const char *sge_root = sge_get_root_dir(0, NULL, 0, 1);
         const char *sge_cell = sge_get_default_cell();

         sprintf(file, "%s/%s/%s", sge_root, sge_cell, 
                 "common/sgeCA/certs/cert.pem");
      }
   } 
   DEXIT;
   return file;
}

static const char*
sge_get_file_priv_key(void)
{
   static bool initialized = false;
   static dstring priv_key = DSTRING_INIT;

   DENTER(TOP_LAYER, "sge_get_file_priv_key");
   if (initialized == false) {
      const char *key = getenv("SGE_KEYFILE");
      
      if (key != NULL) {
         sge_dstring_append(&priv_key, key);
      } else {
         const char *ca_local_dir = "/var/sgeCA"; 
         const char *sge_cell = sge_get_default_cell();
         const char *user_key = "private/key.pem";
         const char *sge_qmaster_port = getenv("SGE_QMASTER_PORT");
     
         if (sge_qmaster_port != NULL) { 
            sge_dstring_sprintf(&priv_key, "%s/port%s/%s/%s", 
                                ca_local_dir, sge_qmaster_port, sge_cell, user_key);
         } else {
            sge_dstring_sprintf(&priv_key, "%s/sge_qmaster/%s/%s", 
                                ca_local_dir, sge_qmaster_port, sge_cell, user_key);
         }
      }
      initialized = true;
   }
   DEXIT;
   return sge_dstring_get_string(&priv_key);
}

static EVP_PKEY * 
read_public_key(const char *certfile)
{
   FILE *fp = NULL;
   X509 *x509;
   EVP_PKEY *pkey;

   DENTER(TOP_LAYER, "read_public_key");
   fp = fopen(certfile, "r");
   if (!fp) {
      DEXIT;
      return NULL;
   }
   x509 = shared_ssl_func__PEM_read_X509(fp, NULL, 0, NULL);
   if (x509 == NULL) {
      shared_ssl_func__ERR_print_errors_fp(stderr);
      DEXIT;
      return NULL;
   }
   fclose (fp);
   pkey = shared_ssl_func__X509_extract_key(x509);
   shared_ssl_func__X509_free(x509);
   if (pkey == NULL) {
      shared_ssl_func__ERR_print_errors_fp(stderr);
   }
   DEXIT;
   return pkey;
}

static EVP_PKEY *
read_private_key(const char *keyfile)
{
   FILE *fp = NULL;
   EVP_PKEY *pkey;

   DENTER(TOP_LAYER, "read_private_key");
   fp = fopen(keyfile, "r");
   if (!fp) {
      DEXIT;
      return NULL;
   }
#if 1
   pkey = shared_ssl_func__PEM_read_PrivateKey(fp, NULL, 0, NULL);
#else
   pkey = PEM_read_PrivateKey(fp, NULL, 0, NULL);
#endif
   fclose (fp);
   if (pkey == NULL) {
      shared_ssl_func__ERR_print_errors_fp(stderr);
   }
   DEXIT;
   return pkey;
}

static void
buffer_append(char **buffer, size_t *buffer_size, size_t *fill_size, 
              char *buffer_append, size_t size_append) 
{
   size_t initial_size = (size_append > 512) ? size_append : 512;

   DENTER(TOP_LAYER, "buffer_append");
   if (*buffer == NULL || *buffer_size == 0) {
      *buffer_size = initial_size;
      *buffer = malloc(initial_size);
      memset(*buffer, 0, *buffer_size);
   } else if (*fill_size + size_append > *buffer_size) {
      *buffer_size += size_append;
      *buffer = realloc(*buffer, *buffer_size);
   }
   memcpy(*buffer + *fill_size, buffer_append, size_append);
   *fill_size += size_append;
   memset(*buffer + *fill_size, 0, *buffer_size - *fill_size);
   DEXIT;
}

#if 0

char *
buffer_read_from_stdin(char **buffer, size_t *size) 
{
   bool repeat = true;
   char *buffer_ptr = NULL;
   size_t buffer_ptr_length = 0;

   DENTER(TOP_LAYER, "buffer_read_from_stdin");
	while (repeat) {
      char buffer[512];
      size_t buffer_length;

		buffer_length = read(0, buffer, sizeof(buffer));
		if (buffer_length <= 0) {
		   if (buffer_length < 0) {
            fprintf(stderr, "sge_passwd: can't read from stdin\n");
            exit(1);
         }
         repeat = false;
		} else {
         if (buffer_ptr == NULL) {
            buffer_ptr = malloc(buffer_length + 1);
         } else {
            buffer_ptr = realloc(buffer_ptr, 
                                 strlen(buffer_ptr) + buffer_length);
         }
         if (buffer_ptr != NULL) {
            memcpy(buffer_ptr + buffer_ptr_length, buffer, buffer_length);
            buffer_ptr_length += buffer_length;
         } else {
            fprintf(stderr, MSG_PWD_MALLOC, prognames[SGE_PASSWD]);
            exit(1);
         }
      }
	}
   *buffer = buffer_ptr;
   *size = (buffer_ptr != NULL) ? strlen(buffer_ptr) : 0;
   DEXIT;
   return buffer_ptr;
}

void
buffer_write_to_stdout(const char *buffer, size_t length)
{
   DENTER(TOP_LAYER, "buffer_write_to_stdout");
   if (buffer != NULL && length > 0) {
      write(1, buffer, length);
   }
   DEXIT;
}

#endif

void 
buffer_encrypt(const char *buffer_in, size_t buffer_in_length, 
               char **buffer_out, size_t *buffer_out_size, 
               size_t *buffer_out_length)
{
	unsigned int ebuflen;
   EVP_CIPHER_CTX ectx;
   unsigned char iv[EVP_MAX_IV_LENGTH];
	unsigned char *ekey[1]; 
	int ekeylen, net_ekeylen; 
	EVP_PKEY *pubKey[1];
	char ebuf[512];

   DENTER(TOP_LAYER, "buffer_encrypt");
   pubKey[0] = read_public_key(sge_get_file_pub_key());
	if(!pubKey[0]) {
      fprintf(stderr, MSG_PWD_LOAD_PUB, prognames[SGE_PASSWD], sge_get_file_pub_key());
      DEXIT;
      exit(1);
   }      

   ekey[0] = malloc(shared_ssl_func__EVP_PKEY_size(pubKey[0]));  
   if (!ekey[0]) {
	   shared_ssl_func__EVP_PKEY_free(pubKey[0]); 
      fprintf(stderr, MSG_PWD_MALLOC, prognames[SGE_PASSWD]);
      DEXIT;
	   exit(1);
	}

 	memset(iv, '\0', sizeof(iv));
#if 0
	shared_ssl_func__EVP_SealInit(&ectx, EVP_des_ede3_cbc(), ekey, &ekeylen, iv, pubKey, 1); 
#else
	shared_ssl_func__EVP_SealInit(&ectx, shared_ssl_func__EVP_rc4(), ekey, &ekeylen, iv, pubKey, 1); 
#endif
	net_ekeylen = htonl(ekeylen);	

   buffer_append(buffer_out, buffer_out_size, buffer_out_length,
                 (char*)&net_ekeylen, sizeof(net_ekeylen));

   buffer_append(buffer_out, buffer_out_size, buffer_out_length,
                 (char*)ekey[0], ekeylen);

   buffer_append(buffer_out, buffer_out_size, buffer_out_length,
                 (char*)iv, sizeof(iv));

   shared_ssl_func__EVP_SealUpdate(&ectx, (unsigned char*)ebuf, 
                                   (int*)&ebuflen, 
                                   (const unsigned char *) buffer_in, 
                                   buffer_in_length);

   buffer_append(buffer_out, buffer_out_size, buffer_out_length,
                 ebuf, ebuflen);

   shared_ssl_func__EVP_SealFinal(&ectx, (unsigned char *)ebuf, (int*)&ebuflen);

   buffer_append(buffer_out, buffer_out_size, buffer_out_length,
                 ebuf, ebuflen);

   shared_ssl_func__EVP_PKEY_free(pubKey[0]);
	free(ekey[0]);
   DEXIT;
}

int
buffer_decrypt(const char *buffer_in, size_t buffer_in_length,
               char **buffer_out, size_t *buffer_out_size,
               size_t *buffer_out_length)

{
	char buf[520];
	char ebuf[512];
	unsigned int buflen;
   EVP_CIPHER_CTX ectx;
   unsigned char iv[EVP_MAX_IV_LENGTH];
	unsigned char *encryptKey; 
	unsigned int ekeylen; 
	EVP_PKEY *privateKey;
   char *curr_ptr = (char*)buffer_in;

   DENTER(TOP_LAYER, "buffer_decrypt");
	memset(iv, '\0', sizeof(iv));
	privateKey = read_private_key(sge_get_file_priv_key());
	if (!privateKey) {
		fprintf(stderr, MSG_PWD_LOAD_PRIV, prognames[SGE_PASSWD], sge_get_file_priv_key());
      DEXIT;
      return 1;
	}

   memcpy(&ekeylen, curr_ptr, sizeof(ekeylen));
   curr_ptr += sizeof(ekeylen);
   buffer_in_length -= sizeof(ekeylen);
	ekeylen = ntohl(ekeylen);
	if (ekeylen != shared_ssl_func__EVP_PKEY_size(privateKey)) {
      shared_ssl_func__EVP_PKEY_free(privateKey);
		fprintf(stderr, MSG_PWD_DECR, prognames[SGE_PASSWD]);
      DEXIT;
      return 1;
	}

	encryptKey = malloc(sizeof(char) * ekeylen);
	if (!encryptKey) {
      shared_ssl_func__EVP_PKEY_free(privateKey);
      fprintf(stderr, MSG_PWD_MALLOC, prognames[SGE_PASSWD]);
      DEXIT;
      return 1;
	}

   memcpy(encryptKey, curr_ptr, ekeylen);
   curr_ptr += ekeylen;
   buffer_in_length -= ekeylen;
   memcpy(&iv, curr_ptr, sizeof(iv));
   curr_ptr += sizeof(iv);
   buffer_in_length -= sizeof(iv);
#if 0
	shared_ssl_func__EVP_OpenInit(&ectx, EVP_des_ede3_cbc(), encryptKey, ekeylen, iv, privateKey); 	
#else
	shared_ssl_func__EVP_OpenInit(&ectx, shared_ssl_func__EVP_rc4(), encryptKey, ekeylen, iv, privateKey); 	
#endif
	while(1) {
      int readlen = 0;

      if (buffer_in_length == 0) {
         break;
      } else if (buffer_in_length < sizeof(ebuf)) {
         memcpy(&ebuf, curr_ptr, buffer_in_length);
         readlen = buffer_in_length;
         buffer_in_length = 0;
      } else {
         memcpy(&ebuf, curr_ptr, sizeof(ebuf));
         curr_ptr += sizeof(ebuf);
         buffer_in_length -= sizeof(ebuf);
         readlen = sizeof(ebuf);
      }

		shared_ssl_func__EVP_OpenUpdate(&ectx, (unsigned char *)buf, 
                                      (int*)&buflen, 
                                      (const unsigned char *)ebuf, readlen);

      buffer_append(buffer_out, buffer_out_size, buffer_out_length,
                    buf, buflen);
	}

   shared_ssl_func__EVP_OpenFinal(&ectx, (unsigned char *)buf, (int*)&buflen);
   buffer_append(buffer_out, buffer_out_size, buffer_out_length,
                 buf, buflen);

   shared_ssl_func__EVP_PKEY_free(privateKey);
	free(encryptKey);
   DEXIT;
   return 0;
}

static int 
password_get_size(const char *filename)
{
   size_t ret = 0;
   FILE *fp = NULL;

   DENTER(TOP_LAYER, "password_get_size");
   fp = fopen(filename, "r");   
   if (fp != NULL) {
      bool do_loop = true;

      while (do_loop) {
         char input[10000];

         if (fscanf(fp, "%[^\n]\n", input) == 1) {
            ret++;
         } else {
            do_loop = false;
         }
      }
      fclose(fp); 
   }
   DEXIT;
   return ret;
}

int
password_read_file(char **users[], char**encryped_pwds[], const char *filename) 
{
   int ret = 0;
   FILE *fp = NULL;

   DENTER(TOP_LAYER, "password_read_file");
   fp = fopen(filename, "r");   
   if (fp != NULL) {
      bool do_loop = true;
      size_t size = password_get_size(filename) + 2;
      size_t i = 0;

      *users = malloc(size * sizeof(char*));
      *encryped_pwds = malloc(size * sizeof(char*));
      while (do_loop) {
         char input[10000];
         char *uname = NULL;
         char *pwd = NULL;

         if (fscanf(fp, "%[^\n]\n", input) == 1) {
            uname = strtok(input, " ");
            pwd = strtok(NULL, " ");
            (*users)[i] = strdup(uname);
            (*encryped_pwds)[i] = strdup(pwd);
            i++;
         } else {
            do_loop = false;
         }
      }
      (*users)[i] = NULL;
      (*encryped_pwds)[i] = NULL; i++;
      (*users)[i] = NULL;
      (*encryped_pwds)[i] = NULL; i++;

      fclose(fp); 
   } else {
      *users = malloc(2 * sizeof(char*));
      *encryped_pwds = malloc(2 * sizeof(char*));
      (*users)[0] = NULL;
      (*encryped_pwds)[0] = NULL;
      (*users)[1] = NULL;
      (*encryped_pwds)[1] = NULL; 

      /* Can't read passwd file */
      ret = 1;
   }
   DEXIT;
   return ret;
}

int
password_find_entry(char *users[], char *encryped_pwds[], const char *user)
{
   int ret = -1;
   size_t i = 0;

   DENTER(TOP_LAYER, "password_find_entry");
   while (users[i] != NULL) {
      if (!strcmp(users[i], user)) {
         ret = i;
         break;
      }
      i++;
   }
   return ret;
}

const char*
sge_get_file_passwd(void)
{
   static char file[4096] = "";

   DENTER(TOP_LAYER, "sge_get_file_passwd");
   if (file[0] == '\0') {
      const char *sge_root = sge_get_root_dir(0, NULL, 0, 1);
      const char *sge_cell = sge_get_default_cell();

      sprintf(file, "%s/%s/common/sgepasswd", sge_root, sge_cell);
   } 
   DEXIT;
   return file;
}

const char*
sge_get_file_dotpasswd(void)
{
   static char file[4096] = "";

   DENTER(TOP_LAYER, "sge_get_file_dotpasswd");
   if (file[0] == '\0') {
      const char *sge_root = sge_get_root_dir(0, NULL, 0, 1);
      const char *sge_cell = sge_get_default_cell();

      sprintf(file, "%s/%s/common/.sgepasswd", sge_root, sge_cell);
   } 
   DEXIT;
   return file;
}

unsigned char *
buffer_encode_hex(unsigned char *input, size_t len, unsigned char **output)
{
   size_t s;

   s = len * 2 + 1;
   *output = malloc(s);
   memset(*output, 0, s);

   for (s = 0; s < len; s++) {
      char buffer[32] = "";
      int byte = input[s];

      sprintf(buffer, "%02x", byte);
      strcat((char*) *output, buffer);
   }
   return *output;
}

unsigned char *
buffer_decode_hex(unsigned char *input, size_t *len, unsigned char **output) 
{
   size_t s;

   s = *len / 2 + 1;
   *output = malloc(s);
   memset(*output, 0, s);

   for (s = 0; s < *len; s+=2) {
      char buffer[32] = "";
      int byte = 0;

      buffer[0] = input[s];
      buffer[1] = input[s+1];

      sscanf(buffer, "%02x", &byte);
      (*output)[s/2] = byte;
   }
   *len = *len / 2;
   return *output;
}

#ifdef DEFINE_SGE_PASSWD_MAIN

static void
password_write_file(char *users[], char *encryped_pwds[], 
                    const char *backup_file, const char *filename) 
{
   FILE *fp = NULL;
   size_t i = 0;

   DENTER(TOP_LAYER, "password_write_file");

   FOPEN(fp, backup_file, "w");
   while (users[i] != NULL) {
      if (users[i][0] != '\0') {
         FPRINTF((fp, "%s %s\n", users[i], encryped_pwds[i]));
      }
      i++;
   }
   FCLOSE(fp);
   rename(backup_file, filename);
   goto FUNC_EXIT;


FOPEN_ERROR:
   fprintf(stderr, MSG_PWD_OPEN_SGEPASSWD, prognames[SGE_PASSWD],
      strerror(errno), errno);
   goto FUNC_EXIT;

FPRINTF_ERROR:
   fprintf(stderr, MSG_PWD_WRITE_SGEPASSWD, prognames[SGE_PASSWD],
      strerror(errno), errno);
   FCLOSE(fp);
   goto FUNC_EXIT;

FCLOSE_ERROR:
   fprintf(stderr, MSG_PWD_CLOSE_SGEPASSWD, prognames[SGE_PASSWD],
      strerror(errno), errno);
   goto FUNC_EXIT;

FUNC_EXIT:
   DEXIT;
}

static void
password_add_or_replace_entry(char **users[], char **encryped_pwds[], 
                              const char *user, const char *encryped_pwd)
{
   size_t i = 0;
   bool done = false;

   DENTER(TOP_LAYER, "password_add_or_replace_entry");
   while ((*users)[i] != NULL) {
      if (!strcmp((*users)[i], user)) {
         free((*encryped_pwds)[i]);
         (*encryped_pwds)[i] = strdup(encryped_pwd);
         done = true;
      }
      i++;
   }
   if (!done) {
      (*users)[i] = strdup(user);
      (*encryped_pwds)[i] = strdup(encryped_pwd); 
   }
   DEXIT;
}

static void
sge_passwd_delete(const char *username, const char *domain)
{
   char only_user[128] = "";
   char user[128] = "";
   char **users = NULL;
   char **encryped_pwd = NULL;
   int i;

   /*
    * Read password table
    */
   password_read_file(&users, &encryped_pwd, sge_get_file_passwd());

   /*
    * Get user name
    */
   if (username != NULL && username[0] != '\0') {
      strcpy(only_user, username);
   } else {
      uid_t uid = getuid();

      if (sge_uid2user(uid, only_user, sizeof(only_user), MAX_NIS_RETRIES)) {
         fprintf(stderr, MSG_PWD_NO_USERNAME, prognames[SGE_PASSWD], (int) uid);
         exit(7);
      }
   }
   if (domain != NULL && domain[0] != '\0') {
      strcpy(user, domain);
      strcat(user, "+");
      strcat(user, only_user);
   } else {
      strcpy(user, only_user);
   }

   /*
    * replace username by zero byte 
    */
   i = password_find_entry(users, encryped_pwd, user);
   if (i != -1) {
      users[i][0] = '\0';
   }

   /* 
    * write new password table 
    */ 
   password_write_file(users, encryped_pwd, 
                       sge_get_file_dotpasswd(), sge_get_file_passwd());
}

#if 0
static void
sge_passwd_show(const char *username) 
{
   char user[128] = "";
   char **users = NULL;
   char **encryped_pwd = NULL;

   DENTER(TOP_LAYER, "sge_passwd_add_change");

   /*
    * Get user name
    */
   if (username != NULL) {
      strcpy(user, username);
   } else {
      uid_t uid = getuid();

      if (sge_uid2user(uid, user, sizeof(user), MAX_NIS_RETRIES)) {
         fprintf(stderr, MSG_PWD_NO_USERNAME, prognames[SGE_PASSWD], uid);
         exit(7);
      }
   
   }

   /*
    * Read password table
    */
   password_read_file(&users, &encryped_pwd, sge_get_file_passwd());

   /*
    * Check if there is an old entry in the password file
    * if it exists then check if the current users knows that pwd
    */
   {
      int i = password_find_entry(users, encryped_pwd, user);

      if (i != -1) {
         unsigned char *buffer_deco = NULL;
         size_t buffer_deco_length = 0;
         char *buffer_decr = NULL;
         size_t buffer_decr_size = 0;
         size_t buffer_decr_length = 0;
         int err64 = 0;

         buffer_deco_length = strlen(encryped_pwd[i]);
         buffer_decode_base64(encryped_pwd[i], &buffer_deco_length, 0, 
                              &err64, &buffer_deco);
         if(buffer_decrypt(buffer_deco, buffer_deco_length, &buffer_decr, 
                        &buffer_decr_size, &buffer_decr_length)!=0) {
            exit(1);
         }

         fprintf(stdout, "%s\n", buffer_decr);

         if (buffer_deco != NULL) {
            free(buffer_deco);
         }
         if (buffer_decr != NULL) {
            free(buffer_decr);
         }
   
      }
   }

   DEXIT;
}
#endif

static void
sge_passwd_add_change(const char *username, const char *domain) 
{
   uid_t uid = getuid();
   char only_user[128] = "";
   char user[128] = "";
   char **users = NULL;
   char **encryped_pwd = NULL;

   DENTER(TOP_LAYER, "sge_passwd_add_change");

   /*
    * Get user name
    */
   if (username != NULL && username[0] != '\0') {
      strcpy(only_user, username);
   } else {
      uid_t uid = getuid();

      if (sge_uid2user(uid, only_user, sizeof(only_user), MAX_NIS_RETRIES)) {
         fprintf(stderr, MSG_PWD_NO_USERNAME, prognames[SGE_PASSWD], (int) uid);
         exit(7);
      }
   }
   if (domain != NULL && domain[0] != '\0') {
      strcpy(user, domain);
      strcat(user, "+");
      strcat(user, only_user);
   } else {
      strcpy(user, only_user);
   }
   
   DPRINTF(("username: %s\n", user));
   fprintf(stdout, MSG_PWD_CHANGE_FOR, user);

   /*
    * Read password table
    */
   password_read_file(&users, &encryped_pwd, sge_get_file_passwd());

   DPRINTF(("read password table\n"));

   /*
    * Check if there is an old entry in the password file and if user is
    * not root if it exists then check if the current users knows that pwd
    */
   if (uid != 0) {
      int i = password_find_entry(users, encryped_pwd, user);

      if (i != -1) {
         char old_passwd[128] = "";
         unsigned char *buffer_deco = NULL;
         size_t buffer_deco_length = 0;
         char *buffer_decr = NULL;
         size_t buffer_decr_size = 0;
         size_t buffer_decr_length = 0;
#if 0
         int err64 = 0;
#endif

         if (shared_ssl_func__EVP_read_pw_string(old_passwd, 128, "Old password: ", 0) != 0) {
            fprintf(stderr, MSG_PWD_CHANGE_ABORT, prognames[SGE_PASSWD]);
            exit(2);
         }  

         buffer_deco_length = strlen(encryped_pwd[i]);
#if 0
         buffer_decode_base64(encryped_pwd[i], &buffer_deco_length, 0, 
                              &err64, &buffer_deco);
#else
         buffer_decode_hex((unsigned char*)encryped_pwd[i], 
                           &buffer_deco_length, &buffer_deco);
#endif

         if(buffer_decrypt((const char*)buffer_deco, buffer_deco_length, 
                        &buffer_decr, 
                        &buffer_decr_size, &buffer_decr_length)!=0) {
            exit(1);
         }
         if (strncmp(buffer_decr, old_passwd, 128)) {
            fprintf(stderr, MSG_PWD_AUTH_FAILURE, prognames[SGE_PASSWD]);
            exit(7);
         }

         DPRINTF(("verified old password\n"));

         if (buffer_deco != NULL) {
            free(buffer_deco);
         }
         if (buffer_decr != NULL) {
            free(buffer_decr);
         }
   
      }
   }

   /*
    * Ask for new password twice and add/replace that password
    */
   {
      char new_passwd[128] = "";
      char new_passwd2[128] = "";
      char *buffer_encr = NULL;
      size_t buffer_encr_size = 0;
      size_t buffer_encr_length = 0;
      unsigned char *buffer_enco = NULL;

      if (shared_ssl_func__EVP_read_pw_string(new_passwd, 128, "New password: ", 0) != 0) {
         fprintf(stderr, MSG_PWD_CHANGE_ABORT, prognames[SGE_PASSWD]);
         exit(2);
      }  
      if (shared_ssl_func__EVP_read_pw_string(new_passwd2, 128, "Re-enter new password: ", 0) != 0) {
         fprintf(stderr, MSG_PWD_CHANGE_ABORT, prognames[SGE_PASSWD]);
         exit(2);
      }  
      if (strncmp(new_passwd, new_passwd2, 128)) {
         fprintf(stderr, MSG_PWD_NO_MATCH, prognames[SGE_PASSWD]);
         exit(7);
      } 

      DPRINTF(("passwords are equivalent\n"));

      if (strlen(new_passwd) == 0) {
         fprintf(stderr, MSG_PWD_INVALID, prognames[SGE_PASSWD]);
         exit(7);
      }

      DPRINTF(("new password is valid\n"));

      buffer_encrypt(new_passwd, strlen(new_passwd), &buffer_encr,
                     &buffer_encr_size, &buffer_encr_length);

#if 0
      buffer_encode_base64(buffer_encr, buffer_encr_length, 0, &buffer_enco);
#else
      buffer_encode_hex((unsigned char*)buffer_encr, 
                        (size_t)buffer_encr_length, &buffer_enco);
#endif

      password_add_or_replace_entry(&users, &encryped_pwd, user, 
                                    (const char *)buffer_enco);
      if (buffer_encr != NULL) {
         free(buffer_encr);
      }
      if (buffer_enco != NULL) {
         free(buffer_enco);
      }
   }

   /* 
    * write new password table 
    */ 
   password_write_file(users, encryped_pwd, 
                       sge_get_file_dotpasswd(), sge_get_file_passwd());
   DPRINTF(("password table has been written\n"));
   fprintf(stdout, MSG_PWD_CHANGED);
   DEXIT;
}

static void 
passwd_become_admin_user(void)
{
   char str[1024];

   DENTER(TOP_LAYER, "passwd_become_admin_user");

   if (sge_set_admin_username(bootstrap_get_admin_user(), str) == -1) {
      fprintf(stderr, SFN": "SFN"\n", prognames[SGE_PASSWD], str);
      exit(1);
   }

   if (sge_switch2admin_user()) {
      fprintf(stderr, MSG_PWD_SWITCH_ADMIN, prognames[SGE_PASSWD]);
      exit(1);
   }

   DEXIT;
   return;
}

static void
sge_passwd_show_usage(void)
{
   DENTER(TOP_LAYER, "sge_passwd_show_usage");
   printf("usage: sgepasswd [[-D domain>] -d user] | [-D domain] [user]\n");
   printf(" [-help]         display this message\n");
   printf(" [-D domain ]    add the given domain name to the user name\n");
   printf(" [-d user ]      delete the password for the named account\n");
   printf(" domain          windows domain name\n");
   printf(" user            windows users without domain name specification\n");
   DEXIT;
}

int main(int argc, char *argv[])
{
   char domain[128] = "";
   char username[128] = "";
   bool do_delete = false;

   DENTER_MAIN(TOP_LAYER, "sgepasswd");

   /* 
    * Do initalisation and switch to admin_user
    */ 
   { 
      char buffer[1024];
      dstring bw;

      if(sge_init_shared_ssl_lib()!=0) {
         exit(1);
      }

      shared_ssl_func__ERR_load_crypto_strings();
      sge_dstring_init(&bw, buffer, sizeof(buffer));
      sge_prof_setup();
      uidgid_mt_init();
      path_mt_init();
      bootstrap_mt_init();
      sge_getme(SGE_PASSWD);

      if (sge_setup_paths(sge_get_default_cell(), &bw) != true) {
         fprintf(stderr, SFN": "SFN"\n", 
                 prognames[SGE_PASSWD], sge_dstring_get_string(&bw));
         exit(1);
      }

      if (sge_bootstrap(&bw) != true) {
         fprintf(stderr, SFN": "SFN"\n", 
                 prognames[SGE_PASSWD], sge_dstring_get_string(&bw));
 
         exit(1);
      }

      passwd_become_admin_user();
   }

   while (argc > 1) {
      if (!strcmp(argv[1],"-help")) {
         argc--; argv++;
         sge_passwd_show_usage();
         DEXIT;
         exit(1);
      } 
      if (!strcmp(argv[1],"-D")) {
         argc--; argv++;
         if (argc != 1 && sscanf(argv[1], "%s", domain) == 1) {
            argc--; argv++;
            continue;
         } else {
            sge_passwd_show_usage();
            DEXIT;
            exit(1);
         }
      }
      if (!strcmp(argv[1],"-d")) {
         uid_t uid = getuid();

         if (uid != 0) {
            fprintf(stderr, MSG_PWD_ONLY_ROOT, prognames[SGE_PASSWD]);
            exit(1);
         }

         argc--; argv++;
         if (argc != 1 && sscanf(argv[1], "%s", username) == 1) {
            argc--; argv++;
            do_delete = true;
            continue;
         } else {
            sge_passwd_show_usage();
            DEXIT;
            exit(1);
         }
      } 
      if (argv[1][0] != '-' && sscanf(argv[1], "%s", username) == 1) {
         uid_t uid = getuid();

         if (uid != 0) {
            fprintf(stderr, MSG_PWD_ONLY_USER, prognames[SGE_PASSWD], username);
            exit(1);
         }
         argc--; argv++;
         continue;
      } 
   }

   if (do_delete) {
      sge_passwd_delete(username, domain);
   } else {
      sge_passwd_add_change(username, domain);
   }

   sge_done_shared_ssl_lib();

   DEXIT;
	return 0;		
}

#endif /* defined( DEFINE_SGE_PASSWD_MAIN ) */
#else  /* defined( SECURE ) */
#if defined( DEFINE_SGE_PASSWD_MAIN )

int main(void)
{
   printf("sgepasswd built with option -no-secure and therefore not functional.\n");
   return 1;
}

#endif /* defined( DEFINE_SGE_PASSWD_MAIN ) */
#endif /* defined( SECURE ) */


