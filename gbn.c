#include "gbn.h"

struct sockaddr global_receiver;
struct sockaddr global_sender;
uint8_t next_seqnum = 0;
size_t data_length;

state_t s;

uint16_t checksum(uint16_t *buf, int nwords)
{
	uint32_t sum;

	for (sum = 0; nwords > 0; nwords--)
		sum += *buf++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

size_t min(size_t a, size_t b) {
    if(b>a)
        return a;
    return b;
}

void timeout(int sig) {
    errno = EINTR;
}

int gbn_socket(int domain, int type, int protocol){

    /*----- Randomizing the seed. This is used by the rand() function -----*/
    srand((unsigned)time(0));

    int sockfd;

    sockfd = socket(domain, type, protocol);
    /* TODO: Your code here. */
    return(sockfd);
}

int gbn_bind(int sockfd, const struct sockaddr *server, socklen_t socklen){

    /* TODO: Your code here. */
    if (bind(sockfd, server, socklen) < 0) {
        return(-1);
    }
    return(1);
}

int gbn_listen(int sockfd, int backlog){

    /* TODO: Your code here. */
    s.current_state = CLOSED;
    return(1);
}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

    /* TODO: Your code here. */

    /*Saving Server Address and Length for Future Use*/
    global_receiver = *server;

    struct gbnhdr SYNPACK = {.type = SYN, .seqnum = 0, .checksum = 0};

    uint16_t new_checksum = checksum((uint16_t*) &SYNPACK,sizeof(SYNPACK) >> 1);
    SYNPACK.checksum = new_checksum;

    if(sendto(sockfd, &SYNPACK, sizeof(SYNPACK), 0, server ,socklen) == -1) {
        perror("SYN Sending failed");
        return (-1);
    }

    printf("SYN Sent successfully.\r\n");
    printf("Sent Data is: %s\r\n", SYNPACK.data);
    printf("SYN Checksum sent is: %u\r\n", SYNPACK.checksum);
    s.current_state = SYN_SENT;

    struct gbnhdr SYNACKPACK;
    if(recvfrom(sockfd, &SYNACKPACK, sizeof(SYNACKPACK), 0, (struct sockaddr*) &server, &socklen) == -1) {
        perror("SYN Ack Recv failed");
        return (-1);
    }
    /*If checksum is correct*/
    uint16_t received_checksum = SYNACKPACK.checksum;
    SYNACKPACK.checksum = 0;
    uint16_t calculated_checksum = checksum((uint16_t*) &SYNACKPACK,sizeof(SYNACKPACK) >> 1);
    if(received_checksum != calculated_checksum) {
        perror("Corrupted Packet");
        return (-1);
    }
    /*Otherwise*/
    printf("SYN Ack Received Successfully.\r\n");
    s.current_state = ESTABLISHED;
    return(1);
}

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t *socklen){

    /* TODO: Your code here. */

    /*Saving Client Address and Length for Future Use*/
    global_sender = *client;

    struct gbnhdr SYNRECPACK;

    if(recvfrom(sockfd, &SYNRECPACK, sizeof(SYNRECPACK), 0, client ,socklen) == -1) {
        perror("SYN Recv failed");
        return (-1);
    }

    /*If checksum is correct*/
    uint16_t received_checksum = SYNRECPACK.checksum;
    SYNRECPACK.checksum = 0;
    uint16_t calculated_checksum = checksum((uint16_t*) &SYNRECPACK,sizeof(SYNRECPACK) >> 1);

    printf("Received Data is: %s\r\n", SYNRECPACK.data);
    printf("Old CS: %u, New CS: %u\r\n", received_checksum, calculated_checksum);

    if(received_checksum != calculated_checksum) {
        perror("Corrupted Packet");
        return (-1);
    }

    printf("SYN Received Successfully.\r\n");
    printf("Type is: %u\r\n", SYNRECPACK.type);
    printf("SYN Checksum received is: %u\r\n", SYNRECPACK.checksum);
    s.current_state = SYN_RCVD;

    /*Need to construct SYN ACK and send it*/
    gbnhdr SYNACKPACK = {.type = SYNACK, .seqnum = 0, .checksum = 0};

    uint16_t synack_checksum = checksum((uint16_t*) &SYNACKPACK,sizeof(SYNACKPACK) >> 1);
    SYNACKPACK.checksum = synack_checksum;

    if(sendto(sockfd, &SYNACKPACK, sizeof(SYNACKPACK), 0, client ,*socklen) == -1) {
        perror("SYN ACK Sending failed");
        return (-1);
    }

    return(sockfd);
}

ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags){

	/* TODO: Your code here. */
	/*Retreiving size of socket address*/
	socklen_t socklen = sizeof(struct sockaddr);

	/* Hint: Check the data length field 'len'.
	 *       If it is > DATALEN, you will have to split the data
	 *       up into multiple packets - you don't have to worry
	 *       about getting more than N * DATALEN.
	 */

    size_t tempLen = len;
    size_t dataLen;
    int bufferIndex = 0;
    data_length = len;

    while(tempLen>0) {
        dataLen = min(tempLen, DATALEN);
        tempLen -= dataLen;
        printf("Ls are: DL: %zd, TL: %zd\r\n", dataLen, tempLen);

        gbnhdr DATAPACK = {.type = DATA, .seqnum = next_seqnum, .checksum = 0};

        memcpy(DATAPACK.data, buf + bufferIndex, dataLen);
        if (dataLen < DATALEN){
            memset(DATAPACK.data+dataLen, 0, DATALEN-dataLen);
        }
        printf("Actual data is: %s\r\n", DATAPACK.data);
        bufferIndex += dataLen;

        /*Calculating New Checksum for DP (Data Packet) and updating the old value*/
        uint16_t new_DPchecksum = checksum((uint16_t *) &DATAPACK, sizeof(DATAPACK) >> 1);
        DATAPACK.checksum = new_DPchecksum;

        printf("Len is: %zd, but sizeOf is: %zd\r\n", len, sizeof(DATAPACK));

        printf("Now printing packet first time: Type is %u, Seqnum is %u, Checksum is %u, and Data is %s.\r\n", DATAPACK.type, DATAPACK.seqnum, DATAPACK.checksum, DATAPACK.data);
        printf("Now printing data length %zd\r\n", sizeof(DATAPACK.data));
        ssize_t sent;
        if ((sent = sendto(sockfd, &DATAPACK, dataLen+4, 0, (struct sockaddr *) &global_receiver, (socklen_t) socklen)) == -1) {
            perror("Data Sending failed");
            return (-1);
        }

        /*TODO: Receive Ack for Data Pack Sent*/
        struct gbnhdr DATAACKPACK;
        if(recvfrom(sockfd, &DATAACKPACK, sizeof(DATAACKPACK), 0, (struct sockaddr*) &global_receiver, &socklen) == -1) {
            perror("Data Ack Recv failed");
            return (-1);
        }
        /*If checksum is correct*/
        uint16_t received_checksum = DATAACKPACK.checksum;
        DATAACKPACK.checksum = 0;
        uint16_t calculated_checksum = checksum((uint16_t*) &DATAACKPACK,sizeof(DATAACKPACK) >> 1);
        printf("Old is: %u but new is %u \r\n", received_checksum, calculated_checksum);
        if(received_checksum != calculated_checksum) {
            perror("Corrupted Packet");
            return (-1);
        }
        /*Otherwise*/

        printf("Current Seq is: %u\r\n", next_seqnum);
        next_seqnum ++;
        printf("Next Seq is: %u\r\n", next_seqnum);
        printf("DATA send is: %zd\r\n", sent);
    }

	return(1);
}

ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags){
	/* TODO: Your code here. */
	/*Retreiving size of socket address*/
	socklen_t socklen = sizeof(struct sockaddr);

    gbnhdr DATAPACK;
    ssize_t recd;

    recd = recvfrom(sockfd, &DATAPACK, len+4, 0, (struct sockaddr*) &global_sender, &socklen);
	if((recd == -1)) {
		perror("Data Recv failed");
		return (-1);
	}

    if(DATAPACK.type == 4) {
        /*TODO: Checksum validation for FIN Packet*/
        printf("FIN Received Successfully.\r\n");
        s.current_state = FIN_RCVD;
        return (0);
    }

    if (recd-4 < DATALEN){
        memset(DATAPACK.data+recd-4, 0, (size_t) DATALEN-(recd-4));
    }

    printf("Now printing packet first time: Type is %u, Seqnum is %u, Checksum is %u, and Data is %s.\r\n", DATAPACK.type, DATAPACK.seqnum, DATAPACK.checksum, DATAPACK.data);
    printf("Now printing data length %zd\r\n", data_length);
    uint16_t received_checksum = DATAPACK.checksum;
    DATAPACK.checksum = 0;
    uint16_t calculated_checksum = checksum((uint16_t*) &DATAPACK,sizeof(DATAPACK) >> 1);
    printf("Now printing packet second time: Type is %u, Seqnum is %u, Checksum is %u, and Data is %s.\r\n", DATAPACK.type, DATAPACK.seqnum, calculated_checksum, DATAPACK.data);
    printf("OLD is: %u, but NEW is: %u\r\n", received_checksum, calculated_checksum);
    printf("Len is: %zd, but sizeOf is: %zd\r\n", len, sizeof(DATAPACK));
    if(received_checksum != calculated_checksum) {
        perror("Corrupted Packet");
        return (-1);
    }

    memcpy(buf, DATAPACK.data, recd-4);

    /*Constructing Data Ack Packet*/
    printf("We need to print received seqnum: %u\r\n", DATAPACK.seqnum);
    gbnhdr DATAACKPACK = {.type = DATAACK, .seqnum = DATAPACK.seqnum, .checksum = 0};
    /*Calculating New Checksum for DAP (Data Ack Packet) and updating the old value*/
    uint16_t new_DAPchecksum = checksum((uint16_t *) &DATAACKPACK, sizeof(DATAACKPACK) >> 1);
    DATAACKPACK.checksum = new_DAPchecksum;
    /*Sending Data Ack Packet*/
    if (sendto(sockfd, &DATAACKPACK, sizeof(DATAACKPACK), 0, (struct sockaddr*) &global_sender, socklen) == -1) {
        perror("Data Ack Sending failed");
        return (-1);
    }

    printf("receiver buf1 size is: %zd\r\n", recd-4);
	return(recd-4);
}

int gbn_close(int sockfd){

	/* TODO: Your code here. */
	printf("We are in gbn_Close and the state is: %u\r\n", s.current_state);
    printf("SYNRCVD is: %u\n", SYN_RCVD);

	/*Retreiving size of socket address*/
	socklen_t socklen = sizeof(struct sockaddr);

	/*Method used by both Sender and Receiver, therefore we need to check FSM State to know what to do*/
	/*If the state is not Established but is FIN_SENT, that means the sender already sent the FIN and
	 *now it is the server's job to receive it and ack it*/

	if(s.current_state == FIN_RCVD){

        printf("Attempting FIN Received Successfully.\r\n");
		/*struct gbnhdr FINRECPACK;
		if(recvfrom(sockfd, &FINRECPACK, sizeof(FINRECPACK), 0, (struct sockaddr*) &global_sender, &socklen) == -1) {
			perror("FIN Recv failed");
			return (-1);
		}*/

        /*Constructing and Sending Fin Ack Packet*/
		gbnhdr FINACKPACK = {.type = FINACK, .seqnum = 0, .checksum = 0};
		/*Calculating checksum and replacing it in packet (which had 0 for checksum)*/
		uint16_t new_FAchecksum = checksum((uint16_t *) &FINACKPACK, sizeof(FINACKPACK) >> 1);
		FINACKPACK.checksum = new_FAchecksum;

		/*Attempting to send FINACKPACK*/
		if (sendto(sockfd, &FINACKPACK, 4, 0, (struct sockaddr*) &global_sender, socklen) == -1) {
			perror("FIN Ack Sending failed");
			return (-1);
		}

		/*If there was no error, print Success and change state of FSM*/
		printf("FIN Ack Sent successfully.\r\n");
		printf("FIN Ack Checksum sent is: %u\r\n", FINACKPACK.checksum);
		s.current_state = FIN_RCVD;
        close(sockfd);

	}

	/* If the state is Established, then this is the Sender moving to close the connection*/

	if(s.current_state == ESTABLISHED) {
		gbnhdr FINPACK = {.type = FIN, .seqnum = 0, .checksum = 0};

		/*Calculating checksum and replacing it in packet (which had 0 for checksum)*/
		uint16_t new_checksum = checksum((uint16_t *) &FINPACK, sizeof(FINPACK) >> 1);
		FINPACK.checksum = new_checksum;

		/*Attempting to send FINPACK*/
		if (sendto(sockfd, &FINPACK, 4, 0, &global_receiver, socklen) == -1) {
			perror("FIN Sending failed");
			return (-1);
		}
		/*If there was no error, print Success and change state of FSM*/
		printf("FIN Sent successfully.\r\n");
		printf("FIN Checksum sent is: %u\r\n", FINPACK.checksum);
		s.current_state = FIN_SENT;
        close(sockfd);
	}

	return(1);
}

ssize_t maybe_sendto(int  s, const void *buf, size_t len, int flags, \
                     const struct sockaddr *to, socklen_t tolen){

	char *buffer = malloc(len);
	memcpy(buffer, buf, len);
	
	
	/*----- Packet not lost -----*/
	if (rand() > LOSS_PROB*RAND_MAX){
		/*----- Packet corrupted -----*/
		if (rand() < CORR_PROB*RAND_MAX){
			
			/*----- Selecting a random byte inside the packet -----*/
			int index = (int)((len-1)*rand()/(RAND_MAX + 1.0));

			/*----- Inverting a bit -----*/
			char c = buffer[index];
			if (c & 0x01)
				c &= 0xFE;
			else
				c |= 0x01;
			buffer[index] = c;
		}

		/*----- Sending the packet -----*/
		int retval = sendto(s, buffer, len, flags, to, tolen);
		free(buffer);
		return retval;
	}
	/*----- Packet lost -----*/
	else
		return(len);  /* Simulate a success */
}
