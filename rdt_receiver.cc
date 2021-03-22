/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 * 
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_receiver.h"
#define WINDOWS_SIZE 5
#define HEADER_SIZE 7

static message* use_message;
static packet* ms_buffer;

static int ms_location;
static char* buffer_is_use;

static int expected_packet;

static short checksum(struct packet* pkt) {
    unsigned long sum = 0;
    for(int i = 2; i < RDT_PKTSIZE; i += 2) 
        sum += *(short *)(&(pkt->data[i]));
    while(sum >> 16) 
        sum = (sum >> 16) + (sum & 0xffff);
    return ~sum;
}

static void respond_ack(int ack) {
    packet ack_packet;
    memcpy(ack_packet.data + sizeof(short), &ack, sizeof(int));
    short sum = checksum(&ack_packet);
    memcpy(ack_packet.data, &sum, sizeof(short));
    Receiver_ToLowerLayer(&ack_packet);
}


/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
    use_message = (message *)malloc(sizeof(message));
    expected_packet = 0;
    ms_location = 0;
    ms_buffer = (packet *)malloc(WINDOWS_SIZE * sizeof(packet));
    buffer_is_use = (char *)malloc(WINDOWS_SIZE);
    for(int i = 0; i < WINDOWS_SIZE; i++)
        buffer_is_use[i]=0;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
    free(use_message);
    free(ms_buffer);
    free(buffer_is_use);
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    short sum;
    memcpy(&sum, pkt->data, sizeof(short));
    if(sum != checksum(pkt)) 
        return;
    int packet_seq;
    memcpy(&packet_seq, pkt->data + sizeof(short), sizeof(int));
    if(packet_seq >= expected_packet && packet_seq < expected_packet + WINDOWS_SIZE) {
        memcpy(&(ms_buffer[packet_seq % WINDOWS_SIZE].data), pkt->data, RDT_PKTSIZE);
        buffer_is_use[packet_seq % WINDOWS_SIZE] = 1;
    }
    else {
        respond_ack(expected_packet - 1);
        return;
    }
    if(packet_seq != expected_packet)
        return;
    
    int payload_size = 0;
    int buffer_index = expected_packet % WINDOWS_SIZE;
    while(buffer_is_use[buffer_index]){
        buffer_is_use[buffer_index] = 0;
        pkt = &ms_buffer[buffer_index];
        payload_size = pkt->data[HEADER_SIZE - 1];
        memcpy(&packet_seq, pkt->data + sizeof(short), sizeof(int));
        if (ms_location == 0) {
            if (use_message->size != 0) {
                free(use_message->data);
            }
            payload_size -= sizeof(int);
            memcpy(&(use_message->size), pkt->data + HEADER_SIZE, sizeof(int));
            use_message->data = (char *)malloc(use_message->size);
            memcpy(use_message->data, pkt->data + HEADER_SIZE + sizeof(int), payload_size);
            ms_location += payload_size;       
        }
        else{
            memcpy(use_message->data + ms_location, pkt->data + HEADER_SIZE, payload_size);
            ms_location += payload_size;                
        } 
        if (ms_location == use_message->size) {
            ms_location = 0;
            Receiver_ToUpperLayer(use_message);
        }
        expected_packet++;
        buffer_index = expected_packet % WINDOWS_SIZE;
    }
    respond_ack(packet_seq);
}

