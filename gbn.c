#include "gbn.h"

struct sockaddr global_receiver;
struct sockaddr global_sender;
uint8_t next_seqnum = 0;
size_t data_length;
uint8_t success_ack = 500000;
int windowSize = 1;
int bigFileCounter = 0;

int received = 0;
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
    signal(SIGALRM, timeout);
    /*printf("BLABLABLABLABLABLA\r\n");*/
}

int gbn_socket(int domain, int type, int protocol){

    printf("LEARNING: %d\r\n", errno);
    /*----- Randomizing the seed. This is used by the rand() function -----*/
    srand((unsigned)time(0));

    /*Initializing Timeout Signal*/
    signal(SIGALRM, timeout);

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

void constructPackets(void *buffer, size_t length_of_buffer, gbnhdr output[], int number_of_packets) {

    uint8_t seqNum = 0;
    size_t tempLength = length_of_buffer;
    size_t actualDataLength;
    int i;
    for(i = 0; i < number_of_packets; i++) {
        actualDataLength = min(DATALEN, tempLength);
        tempLength -= actualDataLength;
        gbnhdr packet = {.type = DATA, .seqnum = seqNum, .checksum = 0};
        memcpy(packet.data, buffer + DATALEN * (i), actualDataLength);
        /*Calculate Checksum*/
        uint16_t new_DPchecksum = checksum((uint16_t *) &packet, sizeof(packet) >> 1);
        packet.checksum = new_DPchecksum;
        output[i] = packet;
        seqNum =  (seqNum+1)%3;
    }

}

ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags){

    /* TODO: Your code here. */
    /*Retreiving size of socket address*/
    socklen_t socklen = sizeof(struct sockaddr);

    /*int windowSize = 1;*/
    uint8_t nextSeqNum = 0;
    int base = 0;
    int currentPacket = 0;
    int maxNumberOfTries = 5;
    int lastSentPacket = 0;

    int bufferOffset = 0;
    int retr=0;
    int totalNumOfPackets;

    int number_of_packets;
    next_seqnum =0;


    if((len/DATALEN)%DATALEN == 0) {
        number_of_packets = (int) len/DATALEN;
    } else {
        number_of_packets = (int) len / DATALEN + 1;
    }
    if(len<DATALEN) {
        number_of_packets =1;
    }
    totalNumOfPackets = number_of_packets;
    struct gbnhdr allPackets[number_of_packets];

    constructPackets(buf, len, allPackets, number_of_packets);
    int initial = 1;

    while(number_of_packets>0) {

        while(lastSentPacket-base+1 < windowSize || initial){
            bufferOffset++;
            initial = 0;
            gbnhdr DATAPACK = allPackets[lastSentPacket];
            /*Need to compute Actual Length*/
            size_t actualDataLength = DATALEN;
            if (number_of_packets == 1 && len%DATALEN != 0){ /*If it's the last packet padd with 0s*/
                actualDataLength = len%DATALEN;
                memset(DATAPACK.data+actualDataLength, 0, DATALEN-actualDataLength);
            }

            /*send*/
            ssize_t sent;
            if ((sent = sendto(sockfd, &DATAPACK, actualDataLength + 4, 0, (struct sockaddr *) &global_receiver,
                               (socklen_t) socklen)) == -1) {
                perror("CODE RED");
            }

            currentPacket++;
            lastSentPacket++;
            if(lastSentPacket == base) {
                alarm(TIMEOUT);
            }
        }

        /*receive*/
        gbnhdr DATAACKPACK;
        if (recvfrom(sockfd, &DATAACKPACK, sizeof(DATAACKPACK), 0, (struct sockaddr *) &global_receiver,
                     &socklen) == -1) {
            if (errno == EINTR) {
                maxNumberOfTries--;
                /*Reset Connection if 5 Timouts occur*/
                if(maxNumberOfTries == 0) {
                    return (-1);
                }
                /*Reset last sent packet to base and set window size to 1*/
                lastSentPacket = base;
                windowSize = 1;
                initial = 1;
                continue;
            }
        }
        /*Checksum Routine if fails continue*/
        uint16_t received_checksum = DATAACKPACK.checksum;
        DATAACKPACK.checksum = 0;
        uint16_t calculated_checksum = checksum((uint16_t*) &DATAACKPACK,sizeof(DATAACKPACK) >> 1);
        if(calculated_checksum != received_checksum) {
            lastSentPacket = base;
            windowSize = 1;
            initial = 1;
            continue;
        }

        number_of_packets --;
        maxNumberOfTries = 0;
        windowSize = 2;
        base++;
        retr++;
        if(DATAACKPACK.seqnum == allPackets[base+windowSize-1].seqnum ) { /*Means a valid ack has arrived but there is another previous one missing, so take to be a cumulative ack*/
            base++;
        }
        if(base == totalNumOfPackets){ /*Sending Done*/
            break;
        }
        if(base == lastSentPacket) { /*If there are no more packets waiting to be ack'ed, reset the timer*/
            alarm(0);
        } else { /*If we receive an ack and there are still packets that need ack'ing, restart the timer*/
            alarm(TIMEOUT);
        }

    }

    return(1);
}

ssize_t gbn_recv(int sockfd, void *buf, size_t len, int flags){

	/*Retreiving size of socket address*/
	socklen_t socklen = sizeof(struct sockaddr);
    if(bigFileCounter == 1024){
        bigFileCounter = 0;
        next_seqnum = 0;
        success_ack = 200;
    }
    bigFileCounter++;

    int duplicate = 0;
    gbnhdr DATAPACK;
    ssize_t recd = 0;

    /*TODO: Start While Loop*/
    while(1) {
        recd = recvfrom(sockfd, &DATAPACK, len + 4, 0, (struct sockaddr *) &global_sender, &socklen);
        if ((recd == -1)) {
            perror("Data Recv failed");
            return (-1);
        }

        if (DATAPACK.type == 4) {
            printf("FIN Received Successfully.\r\n");
            s.current_state = FIN_RCVD;
            return (0);
        }

        if (recd - 4 < DATALEN) {
            memset(DATAPACK.data + recd - 4, 0, (size_t) DATALEN - (recd - 4));
        }

        uint16_t received_checksum = DATAPACK.checksum;
        DATAPACK.checksum = 0;
        uint16_t calculated_checksum = checksum((uint16_t *) &DATAPACK, sizeof(DATAPACK) >> 1);

        if(received_checksum != calculated_checksum) {
            perror("Corrupted Packet");

        }

        if (DATAPACK.seqnum == next_seqnum && received_checksum == calculated_checksum) {
            success_ack = next_seqnum;
            /*printf("SIZE IS %zd, EXPECTED SEQ IS %u, CURRENT SEQ IS %d\r\n", recd-4, next_seqnum, DATAPACK.seqnum);*/
            memcpy(buf, DATAPACK.data, recd - 4);

            next_seqnum = (next_seqnum+1)%3;
            received++;
            break;
        } else {
            if(DATAPACK.seqnum == success_ack){
                duplicate = 1;
            }
        }
        /*Constructing Data Ack Packet*/
        gbnhdr DATAACKPACK = {.type = DATAACK, .seqnum = success_ack, .checksum = 0};
        /*Calculating New Checksum for DAP (Data Ack Packet) and updating the old value*/
        uint16_t new_DAPchecksum = checksum((uint16_t *) &DATAACKPACK, sizeof(DATAACKPACK) >> 1);
        DATAACKPACK.checksum = new_DAPchecksum;
        /*Sending Data Ack Packet*/
        if (sendto(sockfd, &DATAACKPACK, sizeof(DATAACKPACK), 0, (struct sockaddr*) &global_sender, socklen) == -1) {
            perror("Data Ack Sending failed");
            return (-1);
        }

        if(duplicate) {
            printf("ARE WE EVER HERE?");
            continue;
        }

    }

    gbnhdr DATAACKPACK = {.type = DATAACK, .seqnum = success_ack, .checksum = 0};
    /*Calculating New Checksum for DAP (Data Ack Packet) and updating the old value*/
    uint16_t new_DAPchecksum = checksum((uint16_t *) &DATAACKPACK, sizeof(DATAACKPACK) >> 1);
    DATAACKPACK.checksum = new_DAPchecksum;
    /*Sending Data Ack Packet*/
    if (sendto(sockfd, &DATAACKPACK, sizeof(DATAACKPACK), 0, (struct sockaddr*) &global_sender, socklen) == -1) {
        perror("Data Ack Sending failed");
        return (-1);
    }
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
