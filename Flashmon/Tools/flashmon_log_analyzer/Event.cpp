#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>

#include "Event.h"

using namespace std;

Event::Event(double time, eventType type, uint32_t addr, string taskName)
{
	_time = time;
	_type = type;
	_addr = addr;
	_taskName = taskName;
}

Event::Event(const Event &event)
{
	_time = event._time;
	_type = event._type;
	_addr = event._addr;
	_taskName = event._taskName;
}
		
Event::~Event()
{
}

double Event::getTime()
{
	return _time;
}

eventType Event::getType()
{
	return _type;
}

uint32_t Event::getAddr()
{
	return _addr;
}

string Event::getTaskName()
{
	return _taskName;
}

ostream& operator<<(ostream& os, const Event& e)
{
	os << "Event, time: " << e._time << ", type: " << 
		Event::eventTypeToString(e._type) << ", address: " << e._addr <<
		", task: " << e._taskName;
	return os;
}


/**
 * Returns a vector, set of all the events parsed from the csv file
 * passed as parameter. This file is the output of /proc/flashmon_log
 */
/* static */ vector<Event> Event::createEventListFromCsv(string path)
{
	vector<Event> res;
	string line;
	int lineNbr = 0;
	
	ifstream stream(path.c_str());
	if(stream.is_open())
		while(stream.good() || !stream.eof())
		{
			double time;
			char type_char;
			eventType type;
			uint32_t addr;
			char taskName[32];
			
			
			lineNbr++;
			getline(stream, line);
			
			if(line == "")
				continue;
			
			if( sscanf(line.c_str(), "%lf;%c;%u;%s\n", &time, &type_char, &addr, taskName) != 4)
			{
				cerr << "Error parsing " << path << " line " << lineNbr << endl;
				exit(EXIT_FAILURE); 
			}
			
			if(type_char == 'R')
				type = READ;
			else if(type_char == 'W')
				type = WRITE;
			else if(type_char == 'E')
				type = ERASE;
			else
			{
				cerr << "Unknown type in " << path << "line " << lineNbr << endl;
				exit(EXIT_FAILURE);
			}
			
			res.push_back(Event(time, type, addr, taskName));
		}
	else
	{
		cerr << "Error opening " << path << endl;
		exit(EXIT_FAILURE);
	}
	
	return res;
}

/* static */ string Event::eventTypeToString(eventType e)
{
	string res;
	
	switch(e)
	{
		case READ:
			return "read";
		case WRITE:
			return "write";
		case ERASE:
			return "erase";
	}
	
	return res;
}
