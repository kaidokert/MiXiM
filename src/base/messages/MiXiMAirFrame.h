/*
 * MiXiMAirFrame.h
 *
 *  Created on: 07.03.2012
 *      Author: lindig
 */

#ifndef MIXIMAIRFRAME_H_
#define MIXIMAIRFRAME_H_

#include "messages/AirFrame_m.h"

class MiximAirFrame : public AirFrame {
private:
  void copy(const MiximAirFrame& other) { AirFrame::operator=(other); }

protected:
  // protected and unimplemented operator==(), to prevent accidental usage
  bool operator==(const MiximAirFrame&);

public:
  MiximAirFrame(const char *name=NULL, int kind=0) : AirFrame(name, kind) {}
  MiximAirFrame(const MiximAirFrame& other)        : AirFrame(other) {}
  virtual ~MiximAirFrame() {}
  MiximAirFrame& operator=(const MiximAirFrame& other)
  {
      if (this==&other) return *this;
      copy(other);
      return *this;
  }

  virtual MiximAirFrame *dup() const {return new MiximAirFrame(*this);}
};

inline void doPacking(cCommBuffer *b, MiximAirFrame& obj) {obj.parsimPack(b);}
inline void doUnpacking(cCommBuffer *b, MiximAirFrame& obj) {obj.parsimUnpack(b);}

#endif /* MIXIMAIRFRAME_H_ */
