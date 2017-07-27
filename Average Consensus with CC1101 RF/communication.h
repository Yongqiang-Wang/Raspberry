#include "agent.h"
#include <math.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <string.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include "cc1100_raspi.h"
#include <unistd.h>
#include <thread>
#define NEIGHBORS 1
#define BUFSIZE 2048
#define BUFLEN 2048
#define MYADDR 2		//This is the address allocated to this node, should be different for different nodes
#define STATE_FACTOR 10000
using namespace std;
extern Agent *me = new Agent(string("node1"));
uint8_t Tx_fifo[FIFOBUFFER], Rx_fifo[FIFOBUFFER];
uint8_t My_addr, Tx_addr, Rx_addr, Ptklen, pktlen, Lqi, Rssi;
uint8_t rx_addr, sender, lqi;

int cc1100_freq_select, cc1100_mode_select, cc1100_channel_select;
uint8_t cc1100_debug = 0, tx_retries = 1, rx_demo_addr = 3;
int interval = 1000;
CC1100 cc1100;

int sendRF(char* data_array, int data, int addr) //Send data over air
{
	My_addr = MYADDR;
	rx_demo_addr = addr;
	interval = 1000;
	cc1100_debug = 1;
	cc1100_channel_select = 1;
	cc1100_freq_select = 2;
	cc1100_mode_select = 4;
	tx_retries = 5;
	int payload = 60; //max data RF can send as a frame
	int data_r = data + sizeof(int); //total data to be sent
	char d_size[4];
	int counter = 0;
	memcpy(&d_size[0], &data, sizeof(int));
	if (data_r > payload)
	{
		pktlen = payload + 3; //first send payload amount
		for (int i = 0; i < payload; i++)
		{
			if (i < 4)
			{
				Tx_fifo[i + 3] = d_size[i]; //load size of data into buffer
			}
			else
			{
				Tx_fifo[i + 3] = data_array[counter]; // this is the data i have to send
				counter++;
			}

		}
		data_r = data_r - payload; //set data_r to remaining amount of data
	}
	else
	{
		pktlen = data_r + 3;
	}

	//send the initial packet with packet size
	delay(10);
	cc1100.tx_payload_burst(My_addr, rx_demo_addr, Tx_fifo, pktlen);
	cc1100.transmit();
	//set size of the packet to be sent
	while (data_r > 0)
	{
		if (data_r >= payload)
		{
			pktlen = payload + 3; //first send payload amount
			for (int i = 0; i < payload; i++)
			{
				Tx_fifo[i + 3] = data_array[counter]; // this is the data i have to send
				counter++;
			}
			data_r = data_r - payload; //set data_r to remaining amount of data
		}
		else
		{
			pktlen = data_r + 3;
			for (int i = 0; i < data_r; i++)
			{
				Tx_fifo[i + 3] = data_array[counter]; // this is the data i have to send
				counter++;
			}
			data_r = 0;
		}
		delay(10);
		cc1100.tx_payload_burst(My_addr, rx_demo_addr, Tx_fifo, pktlen);
		cc1100.transmit();
		cc1100.receive();
	}





}
int communicator()
{
	me->long_state = -(long)lround(me->state * STATE_FACTOR);
	// encrypt the state
	paillier_plaintext_t* m_s = paillier_plaintext_from_ui(me->long_state);
	paillier_ciphertext_t* c_s = NULL;
	c_s = paillier_enc(NULL, me->pubKey, m_s,
		paillier_get_rand_devurandom);
	char* hexPubKey = paillier_pubkey_to_hex(me->pubKey); //serialize pub key
	char* hexPrvKey = paillier_prvkey_to_hex(me->prvKey); //---thi is for testing
	char* byteCtxt = (char*)paillier_ciphertext_to_bytes(PAILLIER_BITS_TO_BYTES(me->pubKey->bits) * 2, c_s);//serialize cypher
	int count = 1;//send is one, this is a request
	int s_pub = strlen(hexPubKey);
	int s_prv = strlen(hexPrvKey);
	int s_ctxt = PAILLIER_BITS_TO_BYTES(me->pubKey->bits) * 2;
	int i = 0;
	char type[sizeof(int)];
	memcpy(&type, &count, sizeof(int));
	char sendBuf[BUFSIZE];
	memcpy(&sendBuf[i], &type, sizeof(int)); i += sizeof(int); //add type
	int step = me->step;
    memcpy(&sendBuf[i],&step,sizeof(int)); i+=sizeof(int); //add step index;
	memcpy(&sendBuf[i], &s_pub, sizeof(int)); i += sizeof(int); //add size of public key
	memcpy(&sendBuf[i], &s_ctxt, sizeof(int)); i += sizeof(int); //add size of cypher text
	strcpy(&sendBuf[i], hexPubKey); i += s_pub;
	for (int k = i; k<i + s_ctxt; k++)
	{
		sendBuf[k] = byteCtxt[k - i];//load cypher text into send buffer
	}
	i += s_ctxt;
	
	for(int k=0;k<NEIGHBORS;k++)
	{
		sendRF(sendBuf, i,me->_myVector[k].address); //send data to all neighbors
		delay(100);
	}
	
	
}
int RfServer()
{
	int _step,index;
	My_addr = MYADDR;
	rx_demo_addr = 0;
	interval = 1000;
	cc1100_debug = 1;
	cc1100_channel_select = 1;
	cc1100_freq_select = 2;
	cc1100_mode_select = 4;
	tx_retries = 5;
	wiringPiSetup();
	cc1100.begin(My_addr);
	cc1100.sidle();
	cc1100.set_output_power_level(0);
	cc1100.receive();
	for (;;)
	{
		int payload = 60;
		UNA:
		if (cc1100.packet_available())
		{
			cc1100.rx_fifo_erase(Rx_fifo);
			cc1100.rx_payload_burst(Rx_fifo, pktlen);
			if(pktlen == 0)
			{
				printf("Bad packet \n");
				goto UNA;
			}
			int total_receive_size = 0;
			memcpy(&total_receive_size, &Rx_fifo[3], sizeof(int));
			if(abs(total_receive_size) > 1000)
			goto UNA;
			int rem_trans_addr = Rx_fifo[2];
			int counter = 0;
			int data_r = total_receive_size;
			char buf[BUFSIZE];
		//	cout << "received total size: " << total_receive_size << endl;
		//	printf("RX_FIFO: ");
			for (int i = 7; i < pktlen + 1; i++)
			{
				//printf("%d ", Rx_fifo[i]);
				buf[counter] = Rx_fifo[i];
				data_r--;
				counter++;
			}
			//printf("\r\n");
			while (data_r > 0)
			{
				if (cc1100.packet_available())
				{
				// 	printf("data_r: %d \n", data_r);

					cc1100.rx_fifo_erase(Rx_fifo);
					cc1100.rx_payload_burst(Rx_fifo, pktlen);
					for (int i = 3; i < pktlen + 1; i++)
					{
				//		printf("%d ", Rx_fifo[i]);
						buf[counter] = Rx_fifo[i];
						data_r--;
						counter++;
					}
			//	printf("\r\n");

				}

			}
			for(int a=0;a<NEIGHBORS;a++)
			{
				
				if(me->_myVector[a].address == rem_trans_addr)
				{
					index = a; //what neighbor is this
					
				}
				
			}

			paillier_pubkey_t* _pubKey;
			paillier_ciphertext_t* ctxt1;
			paillier_ciphertext_t* c_res = paillier_create_enc_zero();
			
			int def = 0, s_pub, s_ctxt, s_prv;
			int i = 0;
			memcpy(&def, &buf[i], sizeof(int)); i += sizeof(int); //read type
			if (def == 1)//this is a request
			{
				memcpy(&_step, &buf[i],sizeof(int));i+=sizeof(int);//read step index;
				memcpy(&s_pub, &buf[i], sizeof(int)); i += sizeof(int); //read size of public key
				memcpy(&s_ctxt, &buf[i], sizeof(int)); i += sizeof(int); //read size of cypher text	
				char hexPubKey[BUFSIZE]; bzero(hexPubKey, BUFSIZE);
				char byteCtxt[BUFSIZE]; bzero(byteCtxt, BUFSIZE);
				memcpy(&hexPubKey, &buf[i], s_pub); i += s_pub; //read public key
				hexPubKey[s_pub] = 0;
				for (int k = i; k<i + s_ctxt; k++)
				{
					byteCtxt[k - i] = buf[k];
				}i += s_ctxt;
				
				_pubKey = paillier_pubkey_from_hex(hexPubKey); //recreate public key
				
				ctxt1 = paillier_ciphertext_from_bytes((void*)byteCtxt, PAILLIER_BITS_TO_BYTES(_pubKey->bits) * 2); //recreate cypher text
				if(_step <=me->step) //ensure step synchronization
				{
					me->exchange(_pubKey, ctxt1, c_res,index,_step);
					char *resBytes = (char*)paillier_ciphertext_to_bytes(PAILLIER_BITS_TO_BYTES(_pubKey->bits) * 2, c_res);
				char respBuf[BUFSIZE]; bzero(respBuf, BUFSIZE);
				int type = 2; //This is a response
				int size = PAILLIER_BITS_TO_BYTES(_pubKey->bits) * 2;
				int j = 0;
				memcpy(&respBuf[j], &type, sizeof(int)); j += sizeof(int);
				memcpy(&respBuf[j], &_step, sizeof(int)); j+=sizeof(int);
				memcpy(&respBuf[j], &size, sizeof(int)); j += sizeof(int);
				for (int k = j; k<j + size; k++)
				{
					respBuf[k] = resBytes[k - j];
				}j += size;
				
				cout << "sending response to!" <<rem_trans_addr<< endl;
				sendRF(respBuf, j, rem_trans_addr);
				}	
			}
			else if (def == 2)//this is a response
			{
				cout<<"response received"<<endl;
				memcpy(&_step, &buf[i],sizeof(int));i+=sizeof(int);//read step index;
				memcpy(&s_ctxt, &buf[i], sizeof(int)); i += sizeof(int);//read size
				cout<<"sctxt: "<<s_ctxt<<endl;
				char byteCtxt[s_ctxt]; bzero(byteCtxt, s_ctxt);

				memcpy(&byteCtxt, &buf[i], s_ctxt); //read cypher
				//byteCtxt[s_ctxt] = 0;
				cout<<"here"<<endl;
				ctxt1 = paillier_ciphertext_from_bytes((void*)byteCtxt, PAILLIER_BITS_TO_BYTES(me->pubKey->bits) * 2); //recreate
				cout<<"here2"<<endl;			
int cont = 1;
					long result =0;
					me->diff_state =0;
					result = me->ciphertext_to_long(ctxt1);
				cout<<"reached here 1"<<endl;
					if(me->_myVector[index].step < _step)
					{
						me->_myVector[index].diff =  me->_alphas[_step-1] * result;
						me->_myVector[index].step = _step;
											
					//compare all neighbors are at the same step
                                        for(int c=0;c<NEIGHBORS;c++)
										{
                                                if(me->_myVector[c].step == _step)
                                                {
                                                        cont *= 1;
                                                }
                                                else
                                                {
                                                        cont *= 0;
                                                }
                                        }

                                //this ensures all neighboring nodes have successfully communicated befor proceeding to update state

                                        if(cont)
                                        {
                                                for(int c=0;c<NEIGHBORS;c++)
                                                {
                                                        me->diff_state+=me->_myVector[c].diff;
                                                }
                                                //cout<<"Diff state"<<me->diff_state<<endl;
                                                //cout<<"updating state"<<endl;
                                                  me->updateState();
                                        }

					}		
						
					
											
				}

	
			
      		bzero(buf,BUFSIZE);
					
			   }

			//goto START;
		
			
		}
	
}
