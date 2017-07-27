#include "agent.h"
#include "communication.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <thread>
using namespace std;

#define BUFSIZE 2048
#define BUFLEN 2048
#define SERVICE_PORT 21231
const int numAgents = 2;
const int maxSteps = 40;
  // The agents are stored in a vector
  // The graph (neighbors) is stored in a vector of list
 // vector<Agent*> nodes(numAgents);
 // vector<AgentList> edges(numAgents);

myVector a,b,c,d,e;
int main()
{
 a.address = 1; //address of first neighbor ...
b.address = 2;
//me->identity = 3;
  
  srand(8);
 me->_myVector[0] = a;
 me->_myVector[1] = b;
  
	srand(0);
  char id[32];
  double state;
  thread t1(RfServer);
  
       
      state = (rand() % 1000);
      me->setState(state);
  for(int i=0;i<40;i++)
  {
	  printf("Step %2d: ", i);
	delay(2000);
	communicator(); //initiate exchange
        cout<<"My state is: " << me->state<<endl;
  }
       
    while(1);
  return 0;
}
