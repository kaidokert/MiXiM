package org.mixim.tests.baseMobility;

import org.mixim.tests.TestNode;

// Test host for mobility tests, the host will be used to move it
// outside of borders.
module CrashHost extends TestNode
{
    parameters:
        double numHosts; 		// total number of hosts in the network
        double borderPolicy; 	// this hosts border policy

    submodules:
        mobility: TestBMBaseMobility {
            parameters:
                @display("p=130,130;i=cogwheel2");

        }
}

