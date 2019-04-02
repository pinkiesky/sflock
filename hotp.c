#include <string.h>
#include "openssl/evp.h"

// int example() {
//   struct HotpRuntime runtime;
//   struct HotpData data;

//   hotpLoadDataPath("./hotpSettings", &data);
//   hotpInitRuntime(&runtime, &data);

//   hotpCalculate(&runtime, &data);

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
  fscanf(fp, "%s\n%u\n%u\n%s", hotp->secret, &hotp->timeWindowSec, &hotp->tokenSize, hotp->digestName);
}

int hotpLoadDataPath(char *path, struct HotpData *hotp) {
  FILE *fp = fopen(path, "r");
  hotpLoadDataFile(fp, hotp);
  fclose(fp);
}

int hotpInitRuntime(struct HotpRuntime *runtime, struct HotpData *data) {
  runtime->md = EVP_get_digestbyname(data->digestName);
  if (runtime->md == NULL) {
    return -1;
  }

  runtime->mdctx = EVP_MD_CTX_new();

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

  int i;
  for (i = 0; i < hotp->tokenSize; i++) {
    runtime->value[i] = (runtime->md_value[i] % 10) + '0';
  }
  runtime->value[i + 1] = '\0';

  return 0;
}