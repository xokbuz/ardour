/*
    Copyright (C) 2003-2004 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <algorithm>

#include "pbd/error.h"
#include "ardour/types.h"

#include "time_selection.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

TimelineRange&
TimeSelection::operator[] (uint32_t which)
{
	for (std::list<TimelineRange>::iterator i = begin(); i != end(); ++i) {
		if ((*i).id == which) {
			return *i;
		}
	}
	fatal << string_compose (_("programming error: request for non-existent audio range (%1)!"), which) << endmsg;
	abort(); /*NOTREACHED*/
	return *(new ARDOUR::TimelineRange(0,0,0)); /* keep the compiler happy; never called */
}

bool
TimeSelection::consolidate ()
{
	bool changed = false;

  restart:
	for (std::list<TimelineRange>::iterator a = begin(); a != end(); ++a) {
		for (std::list<Range>::iterator b = begin(); b != end(); ++b) {

			if (&(*a) == &(*b)) {
				continue;
			}

			if (a->coverage (b->start, b->end) != Temporal::OverlapNone) {
				a->start = std::min (a->start, b->start);
				a->end = std::max (a->end, b->end);
				erase (b);
				changed = true;
				goto restart;
			}
		}
	}

	return changed;
}

samplepos_t
TimeSelection::start_sample ()
{
	if (empty()) {
		return 0;
	}

	samplepos_t first = max_samplepos;

	for (std::list<TimelineRange>::iterator i = begin(); i != end(); ++i) {
		if ((*i).start < first) {
			first = (*i).start;
		}
	}
	return first;
}

samplepos_t
TimeSelection::end_sample ()
{
	samplepos_t last = 0;

	/* XXX make this work like RegionSelection: no linear search needed */

	for (std::list<TimelineRange>::iterator i = begin(); i != end(); ++i) {
		if ((*i).end > last) {
			last = (*i).end;
		}
	}
	return last;
}

samplecnt_t
TimeSelection::length_samples()
{
	if (empty()) {
		return 0;
	}

	return end_sample() - start() + 1;
}

timepos_t
TimeSelection::start_time ()
{
	if (empty()) {
		return 0;
	}

	timepos_t first = std::numeric_limits<timepos_t>::max();

	for (std::list<TimelineRange>::iterator i = begin(); i != end(); ++i) {
		if ((*i).start < first) {
			first = (*i).start;
		}
	}
	return first;
}

timepos_t
TimeSelection::end_time()
{
	timepos_t last = std::numeric_limits<timepos_t>::min();

	/* XXX make this work like RegionSelection: no linear search needed */

	for (std::list<TimelineRange>::iterator i = begin(); i != end(); ++i) {
		if ((*i).end() > last) {
			last = (*i).end;
		}
	}
	return last;
}

timecnt_t
TimeSelection::length()
{
	if (empty()) {
		return timecnt_t();
	}

	return timecnt_t (end() - start() + 1);
}
