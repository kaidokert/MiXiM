#ifndef __GMACT1_H__
#define __GMACT1_H__

#include "gmac.h"

class GMacT1 : public GMac {

	// contructor, destructor, module stuff
	Module_Class_Members(GMacT1, GMac, 0);

private:
	static bool parametersInitialised;

protected:
	static unsigned int broadcastThreshold, threshold;

	virtual void initialize();
	virtual void finish();
	virtual void wrapSlotCounter();
	virtual SlotState getCurrentSlotState();
	virtual int slotsUntilWake(int destination);

	class HeaderF : public Header {
		struct {
			MsgType type;
		} _data;
	public:
		HeaderF(MsgType type) { _data.type = type; }
		HeaderF(void *data) { memcpy(&_data, data, sizeof(_data)); }
		virtual void *data() { return &_data; }
		virtual int dataSize() { return sizeof(_data); }
		virtual int extraLength() { return _data.type == MSG_ACK || _data.type == MSG_DATA ? ADDRESS_BYTES : 0; }
		virtual MsgType getType() { return _data.type; }
		virtual ~HeaderF() {}
	};

	virtual Header *newHeader(MsgType type) { return new HeaderF(type); }
	virtual Header *newHeader(void *data) { return new HeaderF(data); }
	virtual ~GMacT1();
};


#endif
