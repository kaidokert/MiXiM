#include "DeciderUWBIREDSyncOnAddress.h"

#include "DeciderUWBIRED.h"
#include "MacPkt_m.h"
#include "AirFrame_m.h"

bool DeciderUWBIREDSyncOnAddress::initFromMap(const ParameterMap& params) {
    bool                         bInitSuccess = true;
    ParameterMap::const_iterator it           = params.find("addr");
    if(it != params.end()) {
        syncAddress = LAddress::L2Type(ParameterMap::mapped_type(it->second).longValue());
    }
    else {
        bInitSuccess = false;
        opp_warning("No addr defined in config.xml for DeciderUWBIREDSyncOnAddress!");
    }
    return DeciderUWBIRED::initFromMap(params) && bInitSuccess;
}

bool DeciderUWBIREDSyncOnAddress::attemptSync(const AirFrame* /*frame*/) {
    if (!currentSignal.isProcessing())
        return false;

	cMessage* encaps = currentSignal.first->getEncapsulatedPacket();
	assert(static_cast<MacPkt*>(encaps));
	MacPkt* macPkt = static_cast<MacPkt*>(encaps);

	return (macPkt->getSrcAddr()==syncAddress);
};


