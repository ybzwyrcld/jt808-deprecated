#include "common/jt808_util.h"

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/jt808_protocol.h"


uint8_t BccCheckSum(const uint8_t *src, const int &len) {
  uint8_t checksum = 0;
  for (int i = 0; i < len; ++i) {
    checksum = checksum ^ src[i];
  }
  return checksum;
}

uint16_t Escape(uint8_t *src, const int &len) {
  uint16_t i;
  uint16_t j;
  uint8_t *buffer = new uint8_t[len * 2];

  memset(buffer, 0x0, len * 2);
  for (i = 0, j = 0; i < len; ++i) {
    if (src[i] == PROTOCOL_SIGN) {
      buffer[j++] = PROTOCOL_ESCAPE;
      buffer[j++] = PROTOCOL_ESCAPE_SIGN;
    } else if (src[i] == PROTOCOL_ESCAPE) {
      buffer[j++] = PROTOCOL_ESCAPE;
      buffer[j++] = PROTOCOL_ESCAPE_ESCAPE;
    } else {
      buffer[j++] = src[i];
    }
  }

  memcpy(src, buffer, j);
  delete [] buffer;
  return j;
}

uint16_t ReverseEscape(uint8_t *src, const int &len) {
  uint16_t i;
  uint16_t j;
  uint8_t *buffer = new uint8_t[len];

  memset(buffer, 0x0, len);
  for (i = 0, j = 0; i < len; ++i) {
    if ((src[i] == PROTOCOL_ESCAPE) && (src[i+1] == PROTOCOL_ESCAPE_SIGN)) {
      buffer[j++] = PROTOCOL_SIGN;
      ++i;
    } else if ((src[i] == PROTOCOL_ESCAPE) &&
               (src[i+1] == PROTOCOL_ESCAPE_ESCAPE)) {
      buffer[j++] = PROTOCOL_ESCAPE;
      ++i;
    } else {
      buffer[j++] = src[i];
    }
  }

  memcpy(src, buffer, j);
  delete [] buffer;
  return j;
}

