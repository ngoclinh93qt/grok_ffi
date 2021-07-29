/*
 *    Copyright (C) 2016-2021 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <vector>
#include <map>

namespace grk
{
class PacketLengthCache
{
  public:
	PacketLengthCache(CodingParams* cp);
	virtual ~PacketLengthCache();
	PacketLengthMarkers* createMarkers(IBufferedStream* strm);
	PacketLengthMarkers* getMarkers(void);
	void deleteMarkers(void);
	PacketInfo* next(void);
	void rewind(void);

  private:
	PacketLengthMarkers* pltMarkers;
	SequentialCache<PacketInfo> packetInfoCache;
	CodingParams* m_cp;
};

} // namespace grk
