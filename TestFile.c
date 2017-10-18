#include "gbn.h"

uint16_t checksum(uint16_t *buf, int nwords)
{
    uint32_t sum;

    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

int main(int argc, char *argv[])
{


    struct gbnhdr SYNPACK = {.type = SYN, .seqnum = 0, .checksum = 0};
    /*SYNPACK->type = SYN;
     SYNPACK->seqnum = 0;
     SYNPACK->checksum = 0;
    */
     uint16_t result = checksum((uint16_t*) &SYNPACK,sizeof(SYNPACK));
    printf("Works1: %u \r\n", SYNPACK.checksum);
    SYNPACK.checksum = result;

    printf("Works2: %u \r\n", result);
    printf("Works3: %u \r\n", SYNPACK.checksum);
    /*fprintf(stdout, result);*/
}
