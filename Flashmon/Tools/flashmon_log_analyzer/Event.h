/**
 * An event logged by flashmon, it corresponds to a unique line in 
 * /proc/flashmon_log
 */

#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>
#include <iostream>
#include <vector>
#include <string>

using namespace std;

typedef enum
{
	READ,
	WRITE,
	ERASE
} eventType;

class Event
{
	public:
		Event(double time, eventType type, uint32_t addr, string taskName);
		Event(const Event &event);
		~Event();
		double getTime();
		eventType getType();
		uint32_t getAddr();
		string getTaskName();
		
		static vector<Event> createEventListFromCsv(string path);
		static string eventTypeToString(eventType e);
		
	private:
		double _time;
		eventType _type;
		uint32_t _addr;
		string _taskName;
		
	friend ostream& operator<<(ostream& os, const Event& e);
};

#endif /* EVENT_H */
