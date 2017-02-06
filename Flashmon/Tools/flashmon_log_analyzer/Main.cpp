#include <iostream>
#include <cstdio>
#include <cstdlib>

#include "Event.h"

using namespace std;

double duration(vector<Event *> events);
double startTime(vector<Event *> events);
int readNum(vector<Event *> events);
int writeNum(vector<Event *> events);
int eraseNum(vector<Event *> events);
string processName(vector<Event *> events);

int main(int argc, char **argv)
{
	vector<Event> events;
	vector<Event *> tmp;
	string lastTaskName = "";
	
	if(argc != 2)
	{
		cout << "Usage : " << argv[0] << " <FLashmon log output>" << endl;
		return EXIT_FAILURE;
	}
	
	cout << "# <start time> -> <end time> (<duration>) : [<process name>], "
		"R/W/E : <page read count>/<page write count>/<block erase count>"
		<< endl;
	
	events = Event::createEventListFromCsv(argv[1]);
	
	for(int i=0; i<(int)events.size(); i++)
	{
		if(events[i].getTaskName() != lastTaskName)
		{
			if(lastTaskName != "")
			{
				cout << startTime(tmp) << " -> " << (startTime(tmp) + duration(tmp))
				<< " (" << duration(tmp) << ") : [" << processName(tmp) << "], R/W/E :"
				<< readNum(tmp) << "/" << writeNum(tmp) << "/" << eraseNum(tmp) << "\n";
				tmp.clear();
			}
			lastTaskName = events[i].getTaskName();
		}
		
		tmp.push_back(&(events[i]));
	}
	
	return EXIT_SUCCESS;
}

string processName(vector<Event *> events)
{
	return events[0]->getTaskName();
}

int readNum(vector<Event *> events)
{
	int res = 0;
	
	for(int i=0; i<(int)events.size(); i++)
		if(events[i]->getType() == READ)
			res++;
			
	return res;
}

int writeNum(vector<Event *> events)
{
	int res = 0;
	
	for(int i=0; i<(int)events.size(); i++)
		if(events[i]->getType() == WRITE)
			res++;
			
	return res;
}

int eraseNum(vector<Event *> events)
{
	int res = 0;
	
	for(int i=0; i<(int)events.size(); i++)
		if(events[i]->getType() == ERASE)
			res++;
			
	return res;
}

double startTime(vector<Event *> events)
{
	return events[0]->getTime();
}

double duration(vector<Event *> events)
{
	double start = startTime(events);
	double stop = events[events.size()-1]->getTime();
	return stop - start;
}
