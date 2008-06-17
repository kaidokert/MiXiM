#include "RandomFrequencyOnlyModel.h"


/**
 * @brief The actual filtering method. This implementation just
 * put some random attenuations over time and frequency into
 * the attenuation mapping.
 */
void RandomFrequencyOnlyModel::filterSignal(Signal& s){
	
	
	/* At first get a new instance of the default Mapping implementation
	 * which is able to represent our attenuation mapping.
	 * the first parameter of "createMapping" gets the DimensionSet the
	 * Mapping should use as domain and the second parameter gets the
	 * interpolation method to be used to calculate the values between
	 * two Mapping entries.
	 * 
	 * Note 1: At the moment only LINEAR interpolation is implemented.
	 * 
	 * Note 2: For now the same interpolation method is used for
	 * interpolation in every dimension. Later it might be possible
	 * to define different interpolation methods for different
	 * dimensions. For example: While linear interpolation of time
	 * makes sence in most cases, using NEAREST whould make more sense
	 * for frequency.
	 */
	Mapping* attMapping = Mapping::createMapping(dimensions, Mapping::LINEAR);
	
	/* Get start and end of the signal to avoid unnecessary calculation
	 * of attenuation.*/ 
	simtime_t sStart = s.getSignalStart();
	
	// Since this mapping does not depend on time, we just set values for
	// the first entry in time-dimension (start of the Signal)
	/*
	simtime_t sEnd = sStart + s.getSignalLength();
	
	simtime_t interval = 0.01; //lets use constant intervals for entries in time
	*/
	
	Argument pos(dimensions); //create an Argument which we will use as parameter
							  //to set the values inside the mapping

	pos.setTime(sStart); // set arguments position in time dimension
	
	for(double freq = 2.412; freq <= 2.472; freq += 0.01){
		
		pos.setArgValue(frequency, freq); // update arguments position in frequency dimension
		
		/*
		//create time entries for this frequency
		for(simtime_t t = sStart; t <= sEnd; t+=interval){
		
			
			pos.setTime(t); // update arguments position in time dimension
		*/	
			/* Create random attenuation between 0.1 and 1.0. 
			 * Since the attenuation value is multiplied to the transmission power mapping 
			 * it should be between 0.0 and 1.0, otherwise it whouldn't be a real
			 * attenuation.*/ 
			double att = (double)rand() / (double)RAND_MAX * 0.9 + 0.1;
			
			attMapping->setValue(pos, att); //put the attenuation at the current position into the mapping			
		/*
		}
		*/
	}
	
	//at last add the created attenuation mapping to the signal
	s.addAttenuation(attMapping);
}
