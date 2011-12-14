package org.mixim.tests.baseMobility;

import org.mixim.modules.mobility.StationaryMobility;

// %BaseMobility test module.
simple BaseMobilityTest extends StationaryMobility
{
    parameters:
        @class(BaseMobilityTest);
        double borderPolicy;	// border policy to use
        double outX @unit(m);	// point outside playground X
        double outY @unit(m);	// point outside playground Y
        double outZ @unit(m);	// point outside playground Z

}
