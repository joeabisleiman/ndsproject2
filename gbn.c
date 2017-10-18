#include "gbn.h"

/*void constructPacket(gbnhdr *input, char* packet) {
	sprintf(packet, "%u\r\n%u\r\n%u\r\n", input->type, input->seqnum, input->checksum);

	if(input->data == NULL) {
		sprintf(packet, "%u\r\n\r\n", *input->data);
	}
	else {
		sprintf(packet, "\r\n");
	}
} */

struct sockaddr_in server;
struct sockaddr_in client;
state_t s;

void constructPacket(gbnhdr *input, char* packet) {
	snprintf(packet, 4, "%u%u%u", input->type, input->seqnum, input->checksum);

	if(input->data != NULL) {
		snprintf(packet+4, 5, "%u", *input->data);
	}

}

void modifyChecksum(char *packet, uint16_t new_checksum) {
	if (strlen(packet) == 4) {
		snprintf(packet + 3, 4, "%u", new_checksum);
	} else {
		snprintf(packet + 3, 5, "%u", new_checksum);
	}

}


uint16_t checksum(uint16_t *buf, int nwords)
{
	uint32_t sum;

	for (sum = 0; nwords > 0; nwords--)
		sum += *buf++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags){

	/* TODO: Your code here. */

	/* Hint: Check the data length field 'len'.
	 *       If it is > DATALEN, you will have to split the data
	 *       up into multiple packets - you don't have to worry
	 *       about getting more than N * DATALEN.
	 */

	return(-1);
}

ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags){

	/* TODO: Your code here. */

	return(-1);
}

int gbn_close(int sockfd){

	/* TODO: Your code here. */

	return(-1);
}



int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

	/* TODO: Your code here. */


	struct gbnhdr SYNPACK = {.type = SYN, .seqnum = 0, .checksum = 0};

	uint16_t new_checksum = checksum((uint16_t*) &SYNPACK,sizeof(SYNPACK) >> 1);
	SYNPACK.checksum = new_checksum;

	if(sendto(sockfd, &SYNPACK, sizeof(SYNPACK), 0, server ,socklen) == -1) {
		perror("SYN Sending failed");
		return (-1);
	}

	printf("SYN Sent successfully.\r\n");
	printf("Sent Data is: %s", SYNPACK.data);
	printf("SYN Checksum sent is: %u\r\n", SYNPACK.checksum);
	s.current_state = SYN_SENT;

	struct gbnhdr SYNACKPACK;
	if(recvfrom(sockfd, &SYNACKPACK, sizeof(SYNACKPACK), 0, server, &socklen) == -1) {
		perror("SYN Ack Recv failed");
		return (-1);
	}
	/*If checksum is correct*/
	printf("SYN Ack Received Successfully.\r\n");
	s.current_state = ESTABLISHED;
	return(1);
}

int gbn_listen(int sockfd, int backlog){

	/* TODO: Your code here. */
	s.current_state = CLOSED;
	printf("Listening in GBN \r\n");
	return(1);
}

int gbn_bind(int sockfd, const struct sockaddr *server, socklen_t socklen){

	/* TODO: Your code here. */
	if (bind(sockfd, server, socklen) < 0) {
		return(-1);
	}
	return(1);
}	

int gbn_socket(int domain, int type, int protocol){

	/*----- Randomizing the seed. This is used by the rand() function -----*/
	srand((unsigned)time(0));

	int sockfd;

	sockfd = socket(domain, type, protocol);
	/* TODO: Your code here. */
	return(sockfd);
}

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t *socklen){

	/* TODO: Your code here. */

	struct gbnhdr SYNRECPACK;

	if(recvfrom(sockfd, &SYNRECPACK, sizeof(SYNRECPACK), 0, client ,socklen) == -1) {
		perror("SYN Recv failed");
		return (-1);
	}

	/*If checksum is correct*/
	uint16_t received_checksum = SYNRECPACK.checksum;
	SYNRECPACK.checksum = 0;
	uint16_t calculated_checksum = checksum((uint16_t*) &SYNRECPACK,sizeof(SYNRECPACK) >> 1);

	printf("Received Data is: %s", SYNRECPACK.data);
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
