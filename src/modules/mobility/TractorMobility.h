/* -*- mode:c++ -*- ********************************************************/
//
// Copyright (C) 2007 Peterpaul Klein Haneveld
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

#ifndef RECTANGLE_MOBILITY_H
#define RECTANGLE_MOBILITY_H

#include "MiXiMDefs.h"
#include "BaseMobility.h"

/**
 * @brief Tractor movement model. See NED file for more info.
 *
 * NOTE: Does not yet support 3-dimensional movement.
 * @ingroup mobility
 * @author Peterpaul Klein Haneveld
 */
class MIXIM_API TractorMobility : public BaseMobility
{
protected:
	// configuration
	double x1, y1, x2, y2; ///< rectangle bounds of the field
	double dx, dy; ///< dimensions of the field
	double rows; ///< the number of rows that the tractor must take
	// state
	double row_length, row_width;///<the lenght and width of a row

	double path_length;
	double position;

	/** @brief Target position of the host */
	Coord targetPos;

public:
	TractorMobility()
		: BaseMobility()
		, x1(0), y1(0), x2(0), y2(0)
		, dx(0), dy(0)
		, rows(0)
		, row_length(0), row_width(0)
		, path_length(0)
		, position(0)
		, targetPos()
	{}

	/** @brief Initializes mobility model parameters. */
	virtual void initialize(int);

protected:
	/** @brief Move the host */
	virtual void makeMove();

	virtual void fixIfHostGetsOutside();

	/** @brief Maps d to (x,y) coordinates */
	void calculateXY();
};

#endif

