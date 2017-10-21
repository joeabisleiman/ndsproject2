#include "gbn.h"

struct sockaddr global_receiver;
struct sockaddr global_sender;

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
    /*TODO: If checksum is correct*/
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

	gbnhdr DATAPACK = {.type = DATA, .seqnum = 0, .checksum = 0};
	strncpy(DATAPACK.data, (const char *) buf, strlen(buf));
    printf("buf is: %s\r\n", (const char *) buf);
    printf("DATAPACK.data is: %s\r\n", DATAPACK.data);

	/*Calculating checksum and replacing it in packet (which had 0 for checksum)*/
	uint16_t new_FAchecksum = checksum((uint16_t *) &DATAPACK, sizeof(DATAPACK) >> 1);
	DATAPACK.checksum = new_FAchecksum;

	/*Attempting to send DATAKPACK*/
    ssize_t sent;
	if ((sent = sendto(sockfd, &DATAPACK, sizeof(DATAPACK), 0, (struct sockaddr *) &global_receiver, (socklen_t) socklen)) == -1) {
		perror("Data Sending failed");
		return (-1);
	}
    printf("DATA send is: %zd\r\n", sent);
	return(1);
}

ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags){

	/* TODO: Your code here. */
	/*Retreiving size of socket address*/
	socklen_t socklen = sizeof(struct sockaddr);
	gbnhdr DATAPACK;
    ssize_t recd;

	printf("prior data is: %s\r\n", DATAPACK.data);
	if((recd = recvfrom(sockfd, &DATAPACK.data, sizeof(DATAPACK.data), 0, (struct sockaddr*) &global_sender, &socklen) == -1)) {
		perror("Data Recv failed");
		return (-1);
	}
    printf("receiver buf size is: %zd\r\n", recd);
	printf("data received is: %s\r\n", DATAPACK.data);
	return(recd);
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

	if(s.current_state == SYN_RCVD){

		struct gbnhdr FINRECPACK;
		if(recvfrom(sockfd, &FINRECPACK, sizeof(FINRECPACK), 0, (struct sockaddr*) &global_sender, &socklen) == -1) {
			perror("FIN Recv failed");
			return (-1);
		}
		/*TODO: If checksum is correct*/
		printf("FIN Ack Received Successfully.\r\n");

		gbnhdr FINACKPACK = {.type = FINACK, .seqnum = 0, .checksum = 0};
		/*Calculating checksum and replacing it in packet (which had 0 for checksum)*/
		uint16_t new_FAchecksum = checksum((uint16_t *) &FINACKPACK, sizeof(FINACKPACK) >> 1);
		FINACKPACK.checksum = new_FAchecksum;

		/*Attempting to send FINACKPACK*/
		if (sendto(sockfd, &FINACKPACK, sizeof(FINACKPACK), 0, (struct sockaddr*) &global_sender, socklen) == -1) {
			perror("FIN Ack Sending failed");
			return (-1);
		}

		/*If there was no error, print Success and change state of FSM*/
		printf("FIN Ack Sent successfully.\r\n");
		printf("FIN Ack Checksum sent is: %u\r\n", FINACKPACK.checksum);
		s.current_state = FIN_RCVD;

	}

	/* If the state is Established, then this is the Sender moving to close the connection*/

	if(s.current_state == ESTABLISHED) {
		gbnhdr FINPACK = {.type = FIN, .seqnum = 0, .checksum = 0};

		/*Calculating checksum and replacing it in packet (which had 0 for checksum)*/
		uint16_t new_checksum = checksum((uint16_t *) &FINPACK, sizeof(FINPACK) >> 1);
		FINPACK.checksum = new_checksum;

		/*Attempting to send FINPACK*/
		if (sendto(sockfd, &FINPACK, sizeof(FINPACK), 0, &global_receiver, socklen) == -1) {
			perror("FIN Sending failed");
			return (-1);
		}
		/*If there was no error, print Success and change state of FSM*/
		printf("FIN Sent successfully.\r\n");
		printf("FIN Checksum sent is: %u\r\n", FINPACK.checksum);
		s.current_state = FIN_SENT;
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
