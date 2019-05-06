#include <string.h>
#include "openssl/evp.h"

// int example() {
//   struct HotpRuntime runtime;
//   struct HotpData data;

//   int res = hotpLoadDataPath("/home/rglr/.config/sflock.hotp", &data);
//   if (res < 0) {
//     printf("[err] cannot load hotp data from %s\n", "/home/rglr/.config/sflock.hotp");
//     return 1;
//   }

//   res = hotpInitRuntime(&runtime, &data);
//   if (res < 0) {
//     printf("[err] cannot init hmac runtime\n");
//     return 1;
//   }

//   hotpCalculate(&runtime, &data);
//   printf("Token: %s\n", runtime.value);

//   return 0;
// }

struct HotpData {
  char secret[64];
  unsigned int timeWindowSec;
  unsigned int tokenSize;
  char digestName[8];
};

struct HotpRuntime {
  const EVP_MD *md;
  EVP_MD_CTX *mdctx;

  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;

  char value[64];
};

int hotpLoadDataFile(FILE *fp, struct HotpData *hotp) {
  return fscanf(fp, "%s\n%u\n%u\n%s", hotp->secret, &hotp->timeWindowSec, &hotp->tokenSize, hotp->digestName);
}

int hotpLoadDataPath(char *path, struct HotpData *hotp) {
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    return -1;
  }

  hotpLoadDataFile(fp, hotp);
  fclose(fp);

  return 0;
}

int hotpInitRuntime(struct HotpRuntime *runtime, struct HotpData *data) {
  runtime->md = EVP_get_digestbyname(data->digestName);
  if (runtime->md == NULL) {
    return -1;
  }

  runtime->mdctx = EVP_MD_CTX_new();
  if (runtime->mdctx == NULL) {
    return -1;
  }

  return 0;
}

int hotpDestroyRuntime(struct HotpRuntime *runtime) {
  EVP_MD_CTX_free(runtime->mdctx);
  return 0;
}

int hotpCalculate(struct HotpRuntime *runtime, struct HotpData *hotp) {
  unsigned long currentWindow = time(NULL) / hotp->timeWindowSec;

  EVP_DigestInit_ex(runtime->mdctx, runtime->md, NULL);
  EVP_DigestUpdate(runtime->mdctx, &currentWindow, sizeof(currentWindow));
  EVP_DigestUpdate(runtime->mdctx, hotp->secret, strlen(hotp->secret));
  EVP_DigestFinal(runtime->mdctx, runtime->md_value, &runtime->md_len);

  unsigned int i;
  unsigned int step = runtime->md_len / hotp->tokenSize;
  for (i = 0; i < hotp->tokenSize; i++) {
    runtime->value[i] = (runtime->md_value[i * step] % 10) + '0';
  }
  runtime->value[i + 1] = '\0';

  return 0;
}
