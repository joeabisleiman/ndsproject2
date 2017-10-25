ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags){

	/* TODO: Your code here. */
	/*Retreiving size of socket address*/
	socklen_t socklen = sizeof(struct sockaddr);

	/* Hint: Check the data length field 'len'.
	 *       If it is > DATALEN, you will have to split the data
	 *       up into multiple packets - you don't have to worry
	 *       about getting more than N * DATALEN.
	 */

    int windowSize = 1;
    uint8_t nextSeqNum = 0;
    int base = 0;
    int n = 0;
    size_t tempLength = len;
    size_t actualDataLength;
    int bufferOffset = 0;
    int maxNumberOfTries = 5;

    while(tempLength>0) {

        while(n < windowSize){
            /*printf("n is %d\r\n", n);*/
            actualDataLength = min(DATALEN, tempLength);
            tempLength -= actualDataLength;
            /*construct packet*/
            gbnhdr DATAPACK = {.type = DATA, .seqnum = nextSeqNum, .checksum = 0};
            memcpy(DATAPACK.data, buf + DATALEN * bufferOffset, actualDataLength);

            if (actualDataLength < DATALEN){ /*If it's the last packet padd with 0s*/
                memset(DATAPACK.data+actualDataLength, 0, DATALEN-actualDataLength);
            }

            /*Calculate Checksum*/
            uint16_t new_DPchecksum = checksum((uint16_t *) &DATAPACK, sizeof(DATAPACK) >> 1);
            DATAPACK.checksum = new_DPchecksum;

            /*send*/
            ssize_t sent;
            if ((sent = sendto(sockfd, &DATAPACK, actualDataLength + 4, 0, (struct sockaddr *) &global_receiver,
                                     (socklen_t) socklen)) == -1) {
                perror("CODE RED");
            }
            /*printf("We just sent: %zd\r\n", actualDataLength);*/
            nextSeqNum ++;
            if(nextSeqNum == 2){
                nextSeqNum = 0;
            }
            bufferOffset ++;
            if(n == 0) {
                alarm(TIMEOUT);
            }
            n++;
        }

        /*receive*/
        gbnhdr DATAACKPACK;
        if (recvfrom(sockfd, &DATAACKPACK, sizeof(DATAACKPACK), 0, (struct sockaddr *) &global_receiver,
                     &socklen) == -1) {
            if (errno == EINTR) {
                maxNumberOfTries--;
                if(maxNumberOfTries == 0) {
                    return (-1);
                }
                windowSize = 1;
                continue;
            }
        }
        /*Checksum Routine if fails continue*/

        /*if(duplicate) --> Skip the next computations continue*/
        if(DATAACKPACK.seqnum == 500);
        windowSize = 2;
        n--;
        if(DATAACKPACK.seqnum - base > 0) {
            n--;
        }
        base ++;
        if(n == 0) {
            alarm(0);
        } else {
            alarm(TIMEOUT);
        }

    }
	return(1);
}