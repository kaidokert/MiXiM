#include "tmac.h"

Define_Module_Like(TMac, MacClass);

bool TMac::parametersInitialised = false;
unsigned int TMac::frame_time;
unsigned int TMac::listen_timeout;
int TMac::flags, TMac::max_packet_retries;


void TMac::initialize() {
	// run baseclass initialize first:
	Mac::initialize();
	printfNoInfo(PRINT_INIT, "\t\tTMAC initializing...");

	// get params
	if (!parametersInitialised) {
		listen_timeout = getLongParameter("listenTimeout", LISTEN_TIMEOUT);
		//ev << "listen_timeout = "<<listen_timeout<<"\n";
		assert(listen_timeout>0);
	//	frame_time = par("mac2");
	//	if(!frame_time)
		frame_time = getLongParameter("frameTime", FRAME_TIME);
		//ev << "frame_time = "<<frame_time<<endl;
		assert(frame_time >= listen_timeout);
		flags = getLongParameter("flags", 7);
		max_packet_retries = getLongParameter("maxPacketRetries", PACKET_RETRIES);
		parametersInitialised = true;
	}

	// init data
	sched_state = SCHED_STATE_STARTUP;
	time_last_sched = 0;
	proto_state = PROTO_STATE_IDLE;
	proto_next_state = PROTO_STATE_INVALID;
	tx_msg = NULL;
	nav_state = NAV_STATE_CLEAR;
	nav_end_time = 0;
	silent_for = -1;
	rts_contend_time = RTS_CONTEND_TIME;
	active_state = ACTIVE_STATE_SLEEP;
	resync_counter = 0;
	rts_retries = RTS_RETRIES;
	extra_sched_count = 0;
	next_is_own = 0;
	rts_sendfailed = 0;
	in_my_frame = 0;
	allowRetries = true;

	wantsForce = false;

	// set a timer before sending own SYNC
	int syncat = (int)intuniform(frame_time, 15 * frame_time, RNG_MAC);
	setSchedTimeout(syncat);

	// start listening
	setRadioListen();
}

void TMac::finish() {
	Mac::finish();
	printfNoInfo(PRINT_INIT, "\t\tTMac ending...");
}

TMac::~TMac() {
	parametersInitialised = false;
	if (tx_msg)
		delete tx_msg;
}

void TMac::txPacket(Packet *msg) {
	printf(PRINT_MAC, "tmac: txpacket");
	assert(msg);
	//~ printf(PRINT_MAC, "tmac: txpacket: got msg");
	//~ printf(PRINT_MAC, "tmac: txpacket: local_to != node id");
	if(tx_msg) {
		printf(PRINT_MAC, "got message while busy");
		stat_tx_drop++;
		delete msg;
		return;
	}
	packet_retries = max_packet_retries + 1;
	tx_msg = msg;
	evalState();
}

void TMac::setMySchedule(ushort time) {
	// my frame will start in <time>, which means it
	// started at <time>-<frame_time>
	assert(time < frame_time);
	time_last_sched = node->getCurrentTime() + time - frame_time;
	time_next_sched = node->getCurrentTime() + time;
	must_send_sync = 1;
	next_is_own = 1;
	sched_state = SCHED_STATE_ACTIVE;
	printf(PRINT_MAC, "schedule: %d", time_last_sched % frame_time);
	if(time > 0) {
		setSchedTimeout(time);
		resync_counter = uniform(0.0, 1.0) < .25 ? (int)intuniform(1, 2, RNG_MAC) : (int)intuniform(RESYNC_LOW, RESYNC_HIGH, RNG_MAC);
	} else {
		resync_counter = (int)intuniform(RESYNC_LOW, RESYNC_HIGH, RNG_MAC);
		schedTimeout();
	}
}

void TMac::setIdle() {
	proto_state = PROTO_STATE_IDLE;
	evalState();
}

void TMac::setWait(int time) {
	if(time == 0)
		setIdle();
	else {
		printf(PRINT_MAC, "waiting");
		proto_state = PROTO_STATE_WAIT;
		setRadioListen();
		setProtocolTimeout(time);
	}
}

void TMac::gotoBackoff() {
	int t = (int)intuniform(0, BACKOFF_TIME, RNG_MAC);
	setWait(t);
}

void TMac::evalState() {
	if(getRssi() > 0.5)
		kickFrameActive();
	if(proto_state == PROTO_STATE_IDLE && !isReceiving()) {
		// idling
		if(nav_state == NAV_STATE_CLEAR 
				&& active_state == ACTIVE_STATE_ACTIVE
				&& sched_state == SCHED_STATE_ACTIVE) {
			// listening / active state
			if(must_send_sync) {
				printf(PRINT_MAC, "preparing to send SYNC");
				proto_next_state = PROTO_STATE_SEND_SYNC;
				startContending(SYNC_CONTEND_TIME);
				return;
			}
			if(tx_msg && in_my_frame) { 
				if(mustUseCA(tx_msg)) {
					if (allowRetries) {
						printf(PRINT_MAC, "preparing to send RTS");
						// start contending
						proto_next_state = PROTO_STATE_SEND_RTS;
						startContending(rts_contend_time);
					} else {
						printf(PRINT_MAC, "defering retry");
					}
				} else {
					printf(PRINT_MAC, "preparing to send data");
					proto_next_state = PROTO_STATE_SEND_DATA;
					if (getForce() == FORCE_NOSLEEP)
						startContending(MIN_CONTEND_TIME);
					else
						startContending(rts_contend_time);
				}
				return;
			}
			// nothing to do, listen
			if(in_my_frame) 
				printf(PRINT_MAC, "idle listening (my frame)");
			else
				printf(PRINT_MAC, "idle listening (foreign frame)");

			setRadioListen();
		} else if(sched_state == SCHED_STATE_STARTUP) {
			// keep listening
			printf(PRINT_MAC, "idle listening (startup)");
			setRadioListen();
		} else if(nav_state == NAV_STATE_BUSY) {
			if(flags & FLAG_USE_OVERHEARING_AVOIDANCE) {
				printf(PRINT_MAC, "sleeping (avoid overhearing)");
				if (getForce()!=FORCE_NOSLEEP)
					setRadioSleep();
			} else {
				printf(PRINT_MAC, "idle listening (send prohibited)");
				setRadioListen();
			}
		} else {
			assert(active_state == ACTIVE_STATE_SLEEP);
			assert(sched_state == SCHED_STATE_ACTIVE);
			assert(nav_state == NAV_STATE_CLEAR);
			// sleep state
			printf(PRINT_MAC, "idle sleeping");
			in_my_frame = 0;
			if (getForce() != FORCE_NOSLEEP)
				setRadioSleep();
		}
	}
}

int TMac::mustUseCA(Packet * pkt) {
	return pkt->local_to != BROADCAST;
//	return 0;
}


void TMac::startContending(int time) {
	assert(proto_next_state >= 0); // must have something todo
	assert(time >= MIN_CONTEND_TIME); // otherwise problems with tx -> rx switching
	if(nav_state == NAV_STATE_BUSY) {
		printf(PRINT_MAC, "contend: skipping because nav is busy");
		proto_next_state = PROTO_STATE_INVALID;
		gotoBackoff();
	} else {
		proto_state = PROTO_STATE_CONTEND;
		int ctime = (int)intuniform(5, time, RNG_MAC);
		printf(PRINT_MAC, "starting contention, will fire in %d", ctime);
		setRadioListen(); 
		setProtocolTimeout(ctime);
	}
}

void TMac::rxFrame(Packet * msg){
	assert(msg);
	if (!allowRetries && tx_msg && tx_msg->local_to == msg->local_from)
		allowRetries = true;
	
	if(proto_state == PROTO_STATE_WFCTS && 
			(PKT_KIND(msg) != KIND_CTS 
			 || msg->local_to != node->getNodeId()))
	{
		printf(PRINT_MAC, "received packet, but not cts we want");
		cancelTimeout(TIMER_PROTOCOL);
		proto_state = PROTO_STATE_IDLE;
		packet_retries--;
		if (packet_retries == 0)
			txDone(false);
	}
	if(proto_state == PROTO_STATE_WFACK &&
			(PKT_KIND(msg) != KIND_ACK
			 || msg->local_to != node->getNodeId()))
	{
		printf(PRINT_MAC, "received packet, but not ack we want");
		cancelTimeout(TIMER_PROTOCOL);
		proto_state = PROTO_STATE_IDLE;
		packet_retries--;
		if (packet_retries == 0)
			txDone(false);
	}
			
	assert (active_state != ACTIVE_STATE_SLEEP);
	printf(PRINT_MAC, "got incoming...");
	switch(PKT_KIND(msg)) {
		case KIND_SYNC:
			receiveSync(msg);
			break;
		case KIND_RTS:
			receiveRts(msg);
			break;
		case KIND_CTS:
			receiveCts(msg);
			break;
		case KIND_DATA:
			printf(PRINT_MAC, "got incoming data");
			receiveData(msg);
			break;
		case KIND_ACK:
			receiveAck(msg);
			break;
		case KIND_DS:
			receiveDs(msg);
			break;
		case KIND_FRTS:
			receiveFRts(msg);
			break;
		default:
			assert(false); // unknown msg
	}
	kickFrameActive();
	evalState();
}

void TMac::transmitDone(){
	printf(PRINT_MAC, "transmitDone");
	switch(proto_state) {
		case PROTO_STATE_SEND_RTS:
			proto_state = PROTO_STATE_WFCTS;
			setProtocolTimeout(TIMEOUT_WFCTS);
			setRadioListen();
			if(getRssi()>0.5) {
				printf(PRINT_MAC, "sensed comms after rts");
				kickFrameActive();
			}
			else if(rts_retries-- > 0) {
				printf(PRINT_MAC, "rts retry (%d left)", rts_retries);
				kickFrameActive2();
			}
			break;
		case PROTO_STATE_SEND_CTS:
			kickFrameActive(); // other party may not receive this and retry
			proto_state = PROTO_STATE_WFDATA;
			setProtocolTimeout(TIMEOUT_WFDATA);
			setRadioListen();
			break;
		case PROTO_STATE_SEND_DS:
			sendData();
			break;
		case PROTO_STATE_SEND_DATA:
			kickFrameActive();	// may trigger new msg
			assert(tx_msg);
			if(tx_msg->local_to == BROADCAST) {
				txDone(true);
				setIdle();
			} else {
				proto_state = PROTO_STATE_WFACK;
				setProtocolTimeout(TIMEOUT_ACK);
				setRadioListen();
			}
			break;
		case PROTO_STATE_SEND_FRTS:
			setIdle();
			break;
		case PROTO_STATE_SEND_SYNC:
			kickFrameActive(); // may trigger response
			setIdle();
			break;
		case PROTO_STATE_SEND_ACK:
			kickFrameActive();	// may trigger new msg
			setWait(WAIT_TX_ACK);
			break;
		default:
			assert(false); // unknown
	}
}

void TMac::txDone(bool success) {
	// cleanup
	assert(tx_msg);
	if (success)
		stat_tx++;
	else
		stat_tx_drop++;		

	tx_msg->setKind(success ? TX_DONE : TX_FAILED);
	txPacketDone(tx_msg);
	tx_msg = 0;
}

void TMac::rxFailed() {
	kickFrameActive(); // respond to last byte
	evalState();
}

void TMac::rxStarted() {
	kickFrameActive();
	// if we were contending, cancel it
	if(proto_state == PROTO_STATE_CONTEND) {
		printf(PRINT_MAC, "reception started, cancelling contention");
		cancelTimeout(TIMER_PROTOCOL);
		proto_state = PROTO_STATE_IDLE;
		if(proto_next_state == PROTO_STATE_SEND_RTS) {
			rts_contend_time -= RTS_WAIT_BONUS;
			rts_sendfailed++;
			if(rts_contend_time < MIN_RTS_CONTEND_TIME)
				rts_contend_time = MIN_RTS_CONTEND_TIME;
		} else if (proto_next_state == PROTO_STATE_FORCETIME) {
			wantsForce = false;
			forceGrant(false);
		}
		proto_next_state = PROTO_STATE_INVALID; // none
	} else if(proto_state == PROTO_STATE_WFDATA) {
		printf(PRINT_MAC, "received start of packet, cancelling wait-for-data");
		cancelTimeout(TIMER_PROTOCOL);
		proto_state = PROTO_STATE_IDLE;
	} else if(proto_state == PROTO_STATE_WAIT) {
		cancelTimeout(TIMER_PROTOCOL);
		proto_state = PROTO_STATE_IDLE;
	}
}

void TMac::protocolTimeout() {
	int next_state;
	switch(proto_state) {
		case PROTO_STATE_CONTEND:
			assert(proto_next_state >= 0);
			assert(!isReceiving()); // should be cancelled
			// take a rssi sample, to be sure
			// make sure we sample the ether GPH
			setRadioListen();
			if(getRssi()>0.5) { // someone in the air, restart 
				printf(PRINT_MAC, "sensed communication, cancelling");
				gotoBackoff();
				return;
			}
			// start the next state
			next_state = proto_next_state;
			proto_next_state = PROTO_STATE_INVALID;
			switch(next_state) {
				case PROTO_STATE_SEND_SYNC:
					sendSync();
					break;
				case PROTO_STATE_SEND_RTS:
					sendRts();
					break;
				case PROTO_STATE_SEND_CTS:
					sendCts();
					break;
				case PROTO_STATE_SEND_DATA:
					sendData();
					break;
				case PROTO_STATE_SEND_ACK:
					sendAck();
					break;
				case PROTO_STATE_SEND_DS:
					sendDs();
					break;
				case PROTO_STATE_SEND_FRTS:
					sendFRts();
					break;
				case PROTO_STATE_FORCETIME:
					wantsForce = false;
					proto_state = PROTO_STATE_IDLE;
					forceGrant(true);
					break;
				default: assert(false); // invalid
			}
			break;
		case PROTO_STATE_WFCTS:
			printf(PRINT_MAC, "wait-for-cts timeout (%d)", tx_msg->local_to);
			packet_retries--;
			if (packet_retries == 0) {
				txDone(false);
				setIdle();
			} else {
				allowRetries = false;
				gotoBackoff();
			}
			break;
		case PROTO_STATE_WFDATA:
			printf(PRINT_MAC, "wait-for-data timeout");
			setIdle(); // not our data, so no backoff
			break;
		case PROTO_STATE_WFACK:
			printf(PRINT_MAC, "wait-for-ack timeout (%d)", tx_msg->local_to);
			packet_retries--;
			if (packet_retries == 0) {
				txDone(false);
				setIdle();
			} else {
				gotoBackoff();
			}
			break;
		case PROTO_STATE_WAIT:
			setIdle();
			break;
			
		default:
			assert(false); // invalid
	}
}

void TMac::sendSync() {
	Header header;
	
	printf(PRINT_MAC, "sending sync");
	must_send_sync = 0;
	proto_state = PROTO_STATE_SEND_SYNC;
	Packet *msg = new Packet("sync");
	msg->local_from = node->getNodeId();
	header.kind = KIND_SYNC;
	ushort now = node->getCurrentTime();

	ushort stime = time_last_sched + frame_time - now;
	/* Sometimes the end of the sync will overrun the end of the frame
	   (multiple schedule issues). Then advertise the next next frame instead
	   of the next frame. */
	if (stime <= EST_SYNC_TIME)
		stime += frame_time;
	stime -= EST_SYNC_TIME;
	
	assert(stime > 0 && stime < frame_time);
	header.sync = stime;
	msg->setData(MAC_DATA, &header, sizeof(header), SYNC_SIZE);

	setRadioTransmit();
	reg_tx_overhead(msg);
	startTransmit(msg);
}

void TMac::sendRts() {
	Header header;

	rts_contend_time = RTS_CONTEND_TIME;
	rts_sendfailed = 0;
	assert(tx_msg);
	assert(tx_msg->local_to != node->getNodeId());
	printf(PRINT_MAC, "sending rts -> %d", tx_msg->local_to);
	proto_state = PROTO_STATE_SEND_RTS;
	Packet * msg = new Packet("rts");
	msg->local_from = node->getNodeId();
	msg->local_to = tx_msg->local_to;
	header.kind = KIND_RTS;
	header.nav = navRts(tx_msg->length());
	msg->setData(MAC_DATA, &header, sizeof(header), 0);

	setRadioTransmit();
	reg_tx_overhead(msg);
	startTransmit(msg);
}

void TMac::sendFRts() {
	Header header;

	assert(tx_msg);
	printf(PRINT_MAC, "sending frts -> %d", tx_msg->local_to);
	proto_state = PROTO_STATE_SEND_FRTS;
	Packet * msg = new Packet("frts");
	msg->local_from = node->getNodeId();
	msg->local_to = tx_msg->local_to;
	header.kind = KIND_FRTS;
	header.nav = NAV_FRTS(tx_msg->length());
	msg->setLength(0);
	msg->setData(MAC_DATA, &header, sizeof(header), 0);

	setRadioTransmit();
	reg_tx_overhead(msg);
	startTransmit(msg);
}

void TMac::sendCts() {
	Header header;

	printf(PRINT_MAC, "sending cts");
	proto_state = PROTO_STATE_SEND_CTS;
	Packet * msg = new Packet("cts");
	msg->local_from = node->getNodeId();
	assert(cts_to >=0);
	msg->local_to = cts_to;
	header.kind = KIND_CTS;
	header.nav = (ushort)(cts_nav_end - node->getCurrentTime());
	assert(header.nav > 0 && header.nav < 100000);
	msg->setData(MAC_DATA, &header, sizeof(header), 0);

	setRadioTransmit();
	reg_tx_overhead(msg);
	startTransmit(msg);
}

void TMac::sendDs() {
	Header header;

	printf(PRINT_MAC, "sending ds");
	proto_state = PROTO_STATE_SEND_DS;
	Packet * msg = new Packet("ds");
	msg->local_from = node->getNodeId();
	msg->local_to = tx_msg->local_to;
	header.kind = KIND_DS;
	header.nav = NAV_DS(tx_msg->length());
	msg->setData(MAC_DATA, &header, sizeof(header), DS_SIZE);

	setRadioTransmit();
	reg_tx_overhead(msg);
	startTransmit(msg);
}

void TMac::sendData() {
	Header header;

	printf(PRINT_MAC, "sending data");
	proto_state = PROTO_STATE_SEND_DATA;
	assert(tx_msg);
	assert(tx_msg->local_to != node->getNodeId());
	Packet * msg = (Packet *)tx_msg->dup();
	msg -> local_from = node->getNodeId();
	/*char n[50];
	sprintf(n, "data %d", msg->local_to);
	msg->setName(n);*/
	header.kind = KIND_DATA;
	msg->setData(MAC_DATA, &header, sizeof(header), 0);

	setRadioTransmit();
	reg_tx_data(msg);
	startTransmit(msg);
}

void TMac::sendAck() {
	Header header;

	printf(PRINT_MAC, "sending ack");
	proto_state = PROTO_STATE_SEND_ACK;
	Packet * msg = new Packet("ack");
	msg->local_from = node->getNodeId();
	assert(ack_to >= 0);
	msg->local_to = ack_to;
	header.kind = KIND_ACK;
	msg->setData(MAC_DATA, &header, sizeof(header), 0);

	setRadioTransmit();
	reg_tx_overhead(msg);
	startTransmit(msg);
}

void TMac::receiveSync(Packet * msg) {
	assert(msg);
	reg_rx_overhead(msg);
	ushort stime = (ushort)PKT_SYNC(msg);
	delete msg;
	// first sched?
	if(sched_state == SCHED_STATE_STARTUP) {
		cancelTimeout(TIMER_SCHED);
		printf(PRINT_MAC, "received SYNC, following");
		setMySchedule(stime);
		sched_state = SCHED_STATE_ACTIVE;
		return;
	}
	// my sched?
	ushort f_now = node->getCurrentTime() - time_last_sched;
	assert(f_now <= frame_time);
	ushort ft = f_now + stime; // time in my frame
	if(ft > frame_time)
		ft -= frame_time;
	assert(ft <= frame_time);
	if(isSameSchedule(0, ft)) {
		printf(PRINT_MAC, "received SYNC, my schedule");
		return;
	}
	// known sched?
	int i;
	for(i=0; i<extra_sched_count; i++) {
		if(isSameSchedule(extra_sched[i], ft)) {
			printf(PRINT_MAC, "received SYNC (known schedule)");
			return;
		}
	}
	// new sched!
	adoptSchedule(ft);
		
}

void TMac::adoptSchedule(unsigned short ft) {
	assert(ft <= frame_time - ALLOWED_DRIFT);
	assert(ft >= ALLOWED_DRIFT);
	printf(PRINT_MAC, "adopting extra schedule %u", ft);
	if(extra_sched_count == 5) // full
		return;
	extra_sched[extra_sched_count++] = ft;
	must_send_sync = 1;
}

int TMac::isSameSchedule(unsigned short s1, unsigned short s2) {
	return abs((int)s1 - (int)s2) < ALLOWED_DRIFT
		|| (s1 > s2 && abs((int)s1 - (int)s2 - (int)frame_time) < ALLOWED_DRIFT)
		|| (s2 > s1 && abs((int)s1 - (int)s2 + (int)frame_time) < ALLOWED_DRIFT)
		;
}
	
void TMac::receiveRts(Packet * msg) {
	assert(msg->local_to != BROADCAST);
	if(msg->local_to == node->getNodeId()) { 
		reg_rx_overhead(msg);
		if(txPreferred()
			&& (flags & FLAG_USE_IGNORE_RTS)
			&& tx_msg 
			&& in_my_frame
			&& rts_sendfailed > RTS_IGNORE_TRESHOLD
			&& msg->local_from != tx_msg->local_to // deadlock
			&& uniform(0.0, 1.0, RNG_MAC)<.75) 
		{
			assert(tx_msg);
			printf(PRINT_MAC, "received RTS, ignoring (prefer tx)");
			proto_next_state = PROTO_STATE_SEND_RTS;
			startContending(5);
		} else {
			printf(PRINT_MAC, "received RTS, preparing for cts");
			cts_to = msg->local_from;
			cts_nav_end = node->getCurrentTime() + (ushort)PKT_NAV(msg);
			cts_nav_rcv = PKT_NAV(msg);
			cts_nav_t = node->getCurrentTime();
			proto_next_state = PROTO_STATE_SEND_CTS;
			startContending(CTS_CONTEND_TIME);
		}
	} else {
		printf(PRINT_MAC, "received RTS for %d (not for me)", msg->local_to);
		reg_rx_overhear(msg);
		if (flags & FLAG_USE_FRTS)
			updateNav(NAV_CTS);
		else
			updateNav(PKT_NAV(msg));
	}
	delete msg;
}

void TMac::receiveFRts(Packet * msg) {
	if(msg->local_to == node->getNodeId()) {
		reg_rx_overhead(msg);
		printf(PRINT_MAC, "received FRTS from %d", msg->local_from);
		ushort timer_val = time_next_sched == 0 ? 65535 : time_next_sched - node->getCurrentTime();
		if(PKT_NAV(msg) < timer_val) {
			// wake up earlier
			setSchedTimeout(PKT_NAV(msg));
			time_next_sched = node->getCurrentTime() + PKT_NAV(msg);
			next_is_own = 0;
		}
	} else {
		printf(PRINT_MAC, "overheard FRTS for %d", msg->local_to);
		reg_rx_overhear(msg);
	}
	delete msg;
}

void TMac::receiveCts(Packet * msg) {
	assert(msg->local_to != BROADCAST);
	if(msg->local_to == node->getNodeId()) {
		reg_rx_overhead(msg);
		if(proto_state != PROTO_STATE_WFCTS
				|| msg->local_from != tx_msg->local_to) 
		{
			printf(PRINT_MAC, "ignoring unsoll. cts");
		} else {
			cancelTimeout(TIMER_PROTOCOL);
			printf(PRINT_MAC, "received CTS, preparing to send data");
			if(flags & FLAG_USE_FRTS) 
				proto_next_state = PROTO_STATE_SEND_DS;
			else
				proto_next_state = PROTO_STATE_SEND_DATA;
			startContending(DATA_CONTEND_TIME);
		}
	} else {
		printf(PRINT_MAC, "received CTS for %d (not for me)", msg->local_to);
		reg_rx_overhear(msg);
		if(tx_msg 
				&& in_my_frame
				&& (flags & FLAG_USE_FRTS) 
				&& nav_state == NAV_STATE_CLEAR 
				&& tx_msg->local_to != msg->local_to
				&& tx_msg->local_to != msg->local_from) { 
			// let, during DS frame, other node hear I'm there
			printf(PRINT_MAC, "preparing to send frts");
			proto_next_state = PROTO_STATE_SEND_FRTS;
			startContending(DATA_CONTEND_TIME);
		}
		// update nav only now, so FRTS may be send
		updateNav(PKT_NAV(msg));
	}
	delete msg;
}

void TMac::receiveDs(Packet * msg) {
	if(msg->local_to == node->getNodeId()) {
		printf(PRINT_MAC, "received DS");
		reg_rx_overhead(msg);
		proto_state = PROTO_STATE_WFDATA;
		setProtocolTimeout(TIMEOUT_WFDATA);
	} else {
		printf(PRINT_MAC, "overheard DS");
		reg_rx_overhear(msg);
		updateNav(PKT_NAV(msg));
	}
	delete msg;
}

void TMac::receiveAck(Packet * msg) {
	assert(msg->local_to != BROADCAST);
	if(msg->local_to == node->getNodeId()) {
		reg_rx_overhead(msg);
		if(proto_state != PROTO_STATE_WFACK
				|| msg->local_from != tx_msg->local_to) 
		{
			printf(PRINT_MAC, "ignoring unsoll. ack");
		} else {
			cancelTimeout(TIMER_PROTOCOL);
			printf(PRINT_MAC, "received ack");
			txDone(true);
			setWait(WAIT_TX_DONE);
		}
	} else {
		printf(PRINT_MAC, "received ack for %d (not me)", msg->local_to);
		reg_rx_overhear(msg);
	}
	delete msg;
}

void TMac::receiveData(Packet * msg) {
	if(msg->local_to == node->getNodeId()) {
		printf(PRINT_MAC, "received unicast packet, preparing to send ack");
		ack_to = msg->local_from;

		reg_rx_data(msg);
		rxPacket(msg);
		++stat_rx;

		//  send the ack
		proto_next_state = PROTO_STATE_SEND_ACK;
		startContending(ACK_CONTEND_TIME);
	} else if(msg->local_to == BROADCAST) {
		printf(PRINT_MAC, "received broadcast packet");
		reg_rx_data(msg);
		rxPacket(msg);
		++stat_rx;
	} else {
		printf(PRINT_MAC, "overheard data packet");
		reg_rx_overhear(msg);
		// keep silent until ack is sent
		updateNav(NAV_ACK);
		delete msg;
	}
}

void TMac::kickFrameActive() {
	if (allowRetries)
		rts_retries = RTS_RETRIES;
	kickFrameActive2();
}

void TMac::kickFrameActive2() {
	active_state = ACTIVE_STATE_ACTIVE;
	assert(listen_timeout>0);
	setActiveTimeout(listen_timeout);
	//setActiveTimeout(LISTEN_TIMEOUT);
}

void TMac::updateNav(ushort t) {
	assert(t>0);
	ushort now = node->getCurrentTime();
	ushort nav_left = nav_end_time - now;
	if(nav_state ==  NAV_STATE_CLEAR || t > nav_left) {
		printf(PRINT_MAC, "updating NAV, left = %u", (unsigned)t);
		setNavTimeout(t);
		nav_state = NAV_STATE_BUSY;
		nav_end_time = t + now;
	}
}

void TMac::navTimeout(){
	printf(PRINT_MAC, "NAV timer, medium clear now");
	nav_state = NAV_STATE_CLEAR;
	kickFrameActive();
	evalState();
}

void TMac::schedTimeout(){
	switch(sched_state) {
		case SCHED_STATE_STARTUP:
			printf(PRINT_MAC, "no schedule heard, adopting my own");
			printf(PRINT_STATS, "own sched");
			setMySchedule(0);
			evalState();
			break;
		case SCHED_STATE_ACTIVE: {
			if(next_is_own) {
				printf(PRINT_MAC, "start of my frame");
				allowRetries = true;
				in_my_frame = 1;
				time_last_sched = node->getCurrentTime();
				if(--resync_counter == 0) {
					must_send_sync = 1;
					resync_counter = (int)intuniform(RESYNC_LOW, RESYNC_HIGH, RNG_MAC);
				}
				if (wantsForce)
				{
					startContending(RTS_CONTEND_TIME);	
					proto_next_state = PROTO_STATE_FORCETIME;
				}
			} else {
				printf(PRINT_MAC, "start of foreign frame");
			}
			// determine timer to next frame
			ushort ft_current = node->getCurrentTime() - time_last_sched;
			assert(ft_current <= frame_time);
			// start with my own
			int timer_val = frame_time - ft_current;
			next_is_own = 1;
			// and check if others are earlier
			int i;
			for(i=0; i<extra_sched_count; i++) {
				if(extra_sched[i] > ft_current	// still to come
					&& extra_sched[i]-ft_current < timer_val)
				{
					next_is_own = 0; // this sched is first
					timer_val = extra_sched[i]-ft_current;
				}
			}
			assert(timer_val >0 && 
					(unsigned)(ft_current+timer_val) <= frame_time);
			setSchedTimeout(timer_val);
			time_next_sched = node->getCurrentTime() + timer_val;
			kickFrameActive();
			evalState();
			} // case
			break;
		default:
			assert(false);
	}
}

void TMac::activeTimeout() {
	// check rssi before going down
	if(getRssi() > 0.5) {
		printf(PRINT_MAC, "sensed communication at active timeout");
		kickFrameActive();
	} else {
		printf(PRINT_MAC, "active timeout");
		active_state = ACTIVE_STATE_SLEEP;
	}
	evalState();
}

int TMac::headerLength() {
	/* To, from and type fields */
	return 5;
}

void TMac::timeout(int which){
	switch(which) {
		case TIMER_PROTOCOL:
			protocolTimeout();
			break;
		case TIMER_NAV:
			navTimeout();
			break;
		case TIMER_SCHED:
			schedTimeout();
			break;
		case TIMER_ACTIVE:
			activeTimeout();
			break;
		default:
			assert(false); // unknown timer
	}
}

void TMac::setProtocolTimeout(int t) {
	setTimeout(t, TIMER_PROTOCOL);
}

void TMac::setNavTimeout(int t) {
	setTimeout(t, TIMER_NAV);
}

void TMac::setSchedTimeout(int t) {
	setTimeout(t, TIMER_SCHED);
}

void TMac::setActiveTimeout(int t) {
	setTimeout(t, TIMER_ACTIVE);
}

/*
void TMac::incBackoff() {
	rts_contend_time *= 2;
	if(rts_contend_time > MAX_RTS_CONTEND_TIME)
		rts_contend_time = MAX_RTS_CONTEND_TIME;
}

void TMac::decBackoff() {
	rts_contend_time = RTS_CONTEND_TIME;
}
*/

inline int TMac::navRts(int length) {
	if (flags & FLAG_USE_FRTS)
		return CTS_CONTEND_TIME+CTS_FLENGTH+DATA_CONTEND_TIME+DS_FLENGTH+FLENGTH(length)+ACK_CONTEND_TIME+ACK_FLENGTH;
	else 
		return CTS_CONTEND_TIME+CTS_FLENGTH+DATA_CONTEND_TIME+FLENGTH(length)+ACK_CONTEND_TIME+ACK_FLENGTH;
}

void TMac::endForce()
{
	evalState();
}

void TMac::forceRequest()
{
	wantsForce = true;
}

