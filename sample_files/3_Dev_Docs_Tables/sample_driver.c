#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t payload_size;
} OmniHeader;

int validate_header(const OmniHeader* hdr) {
    if (!hdr) return 0;
    return (hdr->magic == 0x5045454B) && (hdr->version >= 1);
}

int main(void) {
    OmniHeader hdr = { .magic = 0x5045454B, .version = 1, .payload_size = 1024 };
    if (validate_header(&hdr)) {
        printf("OmniHeader validation succeeded!\n");
    }
    return 0;
}
