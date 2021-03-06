/* -*- mode:c++ -*- ********************************************************
 * file:        TestStateSub.h
 *
 * author:      Andreas Koepke
 *
 * copyright:   (C) 2004 Telecommunication Networks Group (TKN) at
 *              Technische Universitaet Berlin, Germany.
 *
 *              This program is free software; you can redistribute it 
 *              and/or modify it under the terms of the GNU General Public 
 *              License as published by the Free Software Foundation; either
 *              version 2 of the License, or (at your option) any later 
 *              version.
 *              For further information see file COPYING 
 *              in the top level directory
 ***************************************************************************
 * part of:     testsuite of framework
 * description: vsiualizes state read from signaling system
 ***************************************************************************
 * changelog:   $Revision: 1.2 $
 *              last modified:   $Date: 2004/02/09 13:59:33 $
 *              by:              $Author: omfw-willkomm $
 **************************************************************************/


#ifndef TESTSTATESUB_H
#define TESTSTATESUB_H

#include "BaseModule.h"

class TestStateSub : public BaseModule
{
private:
	/** @brief Copy constructor is not allowed.
	 */
	TestStateSub(const TestStateSub&);
	/** @brief Assignment operator is not allowed.
	 */
	TestStateSub& operator=(const TestStateSub&);

private:
    cModule *host;

public:
    TestStateSub()
    	: BaseModule()
    	, host(NULL)
    {}
    virtual void initialize(int stage);
    virtual void handleMessage( cMessage* );
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj);
};

#endif
