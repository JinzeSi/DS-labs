/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_sender.h"

#define MAX_BUFFER_SIZE 20000
#define MAX_WINDOW_SIZE 5
#define HEADER_SIZE 7
#define TIMEOUT 0.3

#define MAX_PAYLOAD_SIZE (RDT_PKTSIZE - HEADER_SIZE)
//ms=message 
static message* ms_buffer;

static int num_ms;
static int cur_ms_seq;
static int next_ms_seq;
static int ms_location;


static packet* windows;

static int next_packet_seq;
static int next_packet_to_send_seq;
static int ack_expected;
static int num_packet;


static short checksum(struct packet* pkt) {
    unsigned long sum = 0;
    for(int i = 2; i < RDT_PKTSIZE; i += 2) 
        sum += *(short *)(&(pkt->data[i]));
    while(sum >> 16) 
        sum = (sum >> 16) + (sum & 0xffff);
    return ~sum;
}

static void cut_message_and_send() {
    int ms_index = cur_ms_seq % MAX_BUFFER_SIZE;
    message msg_tmp = ms_buffer[ms_index];
    packet packet_tmp;
    short sum;
    while(num_packet < MAX_WINDOW_SIZE && cur_ms_seq < next_ms_seq){
        int size = 0;
        memcpy(packet_tmp.data + sizeof(short), &next_packet_seq, sizeof(int));
        if(msg_tmp.size > MAX_PAYLOAD_SIZE + ms_location){
            size = MAX_PAYLOAD_SIZE;
        }
        else{
            size = msg_tmp.size - ms_location;
        }
        packet_tmp.data[HEADER_SIZE - 1] = size;
        memcpy(packet_tmp.data + HEADER_SIZE, msg_tmp.data + ms_location,size);
        sum = checksum(&packet_tmp);
        memcpy(packet_tmp.data, &sum, sizeof(short));
        int next_packet_index = next_packet_seq % MAX_WINDOW_SIZE;
        memcpy( &(windows[next_packet_index]), &packet_tmp, sizeof(packet));
        next_packet_seq++;
        num_packet++;
        if(msg_tmp.size > MAX_PAYLOAD_SIZE + ms_location){
            ms_location += MAX_PAYLOAD_SIZE;
        }
        else{
            cur_ms_seq++;
            num_ms--;
            ms_location = 0;
            if(cur_ms_seq < next_ms_seq){
                msg_tmp = ms_buffer[cur_ms_seq % MAX_BUFFER_SIZE];
            }
        }
        
    }
    packet packet;
    for(int i = 0;i < MAX_WINDOW_SIZE && next_packet_to_send_seq < next_packet_seq; i++) {
        memcpy(&packet, &(windows[next_packet_to_send_seq % MAX_WINDOW_SIZE]), sizeof(packet));
        Sender_ToLowerLayer(&packet);
        next_packet_to_send_seq++;
    }
}

static void send_packets_again() {
    packet packet_tmp;
    for(int i = 0;i < MAX_WINDOW_SIZE && next_packet_to_send_seq < next_packet_seq; i++) {
        memcpy(&packet_tmp, &(windows[next_packet_to_send_seq % MAX_WINDOW_SIZE]), sizeof(packet));
        Sender_ToLowerLayer(&packet_tmp);
        next_packet_to_send_seq++;
    }
}



/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    ms_buffer = (message*) malloc((MAX_BUFFER_SIZE) * sizeof(message));
    memset(ms_buffer, 0, (MAX_BUFFER_SIZE) * sizeof(message));
    cur_ms_seq = 0;
    next_ms_seq = 0;
    num_ms = 0;
    
    ms_location = 0;

    windows = (packet*) malloc((MAX_WINDOW_SIZE) * sizeof(packet));
    
    next_packet_seq = 0;
    next_packet_to_send_seq = 0;
    ack_expected = 0;
    num_packet = 0;
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
    for (int i = 0; i < MAX_BUFFER_SIZE; ++i) {
        if (ms_buffer[i].size != 0) {
            free(ms_buffer[i].data);
        }
    }
    free(ms_buffer);
    free(windows);
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{

    if (num_ms >= MAX_BUFFER_SIZE) {
        ASSERT(0);
    }
    int next_ms_index = next_ms_seq % MAX_BUFFER_SIZE;
    if (ms_buffer[next_ms_index].size != 0) {
        ms_buffer[next_ms_index].size = 0;
        free(ms_buffer[next_ms_index].data);
    }
    
    ms_buffer[next_ms_index].size = msg->size + sizeof(int); 
    ms_buffer[next_ms_index].data = (char *)malloc(msg->size + sizeof(int));
    memcpy(ms_buffer[next_ms_index].data, &(msg->size), sizeof(int)); 
    memcpy(ms_buffer[next_ms_index].data + sizeof(int), msg->data, msg->size); 
    next_ms_seq++;
    num_ms++;

    if (Sender_isTimerSet() ) {
        return;
    }
    Sender_StartTimer(TIMEOUT);
    cut_message_and_send();
   
    
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    short sum;
    memcpy(&sum, pkt->data, sizeof(short));
    if(sum != checksum(pkt)) 
        return;

    int ack;
    memcpy(&ack, pkt->data + sizeof(short), sizeof(int));
    if(ack_expected <= ack && ack < next_packet_seq){
        Sender_StartTimer(TIMEOUT);
        num_packet -= (ack - ack_expected + 1);
        ack_expected = ack + 1;
        cut_message_and_send();
       
    }

    if(ack == next_packet_seq - 1) 
        Sender_StopTimer();
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    Sender_StartTimer(TIMEOUT);
    next_packet_to_send_seq = ack_expected;
    send_packets_again();
}
