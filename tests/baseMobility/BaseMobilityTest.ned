package org.mixim.tests.baseMobility;

import org.mixim.base.modules.BaseMobility;

simple BaseMobilityTest extends BaseMobility
{
    parameters:
        @class(BaseMobilityTest);
        double borderPolicy;	// border policy to use
        double outX @unit(m);			// point outside playground
        double outY @unit(m);
        double outZ @unit(m);

}
