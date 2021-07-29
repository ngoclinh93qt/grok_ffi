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
struct MarkerInfo
{
	MarkerInfo(uint16_t _id, uint64_t _pos, uint32_t _len);
	MarkerInfo();
	void dump(FILE* outputFileStream);
	/** marker id */
	uint16_t id;
	/** position in code stream */
	uint64_t pos;
	/** length (marker id included) */
	uint32_t len;
};
struct TilePartInfo
{
	TilePartInfo(uint64_t start, uint64_t endHeader, uint64_t end);
	TilePartInfo(void);
	void dump(FILE* outputFileStream, uint8_t tilePart);
	/** start position of tile part*/
	uint64_t startPosition;
	/** end position of tile part header */
	uint64_t endHeaderPosition;
	/** end position of tile part */
	uint64_t endPosition;
};
struct TileInfo
{
	TileInfo(void);
	~TileInfo(void);
	bool checkResize(void);
	bool hasTilePartInfo(void);
	bool update(uint16_t tileIndex, uint8_t currentTilePart, uint8_t numTileParts);
	TilePartInfo* getTilePartInfo(uint8_t tilePart);
	void dump(FILE* outputFileStream, uint16_t tileNum);
	/** tile index */
	uint16_t tileno;
	/** number of tile parts */
	uint8_t numTileParts;
	/** current nb of tile part (allocated)*/
	uint8_t allocatedTileParts;
	/** current tile-part index */
	uint8_t currentTilePart;

  private:
	/** tile part index */
	TilePartInfo* tilePartInfo;
	/** array of markers */
	MarkerInfo* markerInfo;
	/** number of markers */
	uint32_t numMarkers;
	/** actual size of markers array */
	uint32_t allocatedMarkers;
};
struct CodeStreamInfo
{
	CodeStreamInfo(IBufferedStream* str);
	virtual ~CodeStreamInfo();
	bool allocTileInfo(uint16_t numTiles);
	bool updateTileInfo(uint16_t tileIndex, uint8_t currentTilePart, uint8_t numTileParts);
	TileInfo* getTileInfo(uint16_t tileIndex);
	void dump(FILE* outputFileStream);
	void pushMarker(uint16_t id, uint64_t pos, uint32_t len);
	uint64_t getMainHeaderStart(void);
	void setMainHeaderStart(uint64_t start);
	uint64_t getMainHeaderEnd(void);
	void setMainHeaderEnd(uint64_t end);
	bool skipToTile(uint16_t tileIndex, uint64_t lastSotReadPosition);

  private:
	/** main header start position (SOC position) */
	uint64_t mainHeaderStart;
	/** main header end position (first SOT position) */
	uint64_t mainHeaderEnd;
	/** list of markers */
	std::vector<MarkerInfo*> marker;
	uint16_t numTiles;
	TileInfo* tileInfo;
	IBufferedStream* stream;
};
struct TilePartLengthInfo
{
	TilePartLengthInfo() : hasTileIndex(false), tileIndex(0), length(0) {}
	TilePartLengthInfo(uint32_t len) : hasTileIndex(false), tileIndex(0), length(len) {}
	TilePartLengthInfo(uint16_t tileno, uint32_t len)
		: hasTileIndex(true), tileIndex(tileno), length(len)
	{}
	bool hasTileIndex;
	uint16_t tileIndex;
	uint32_t length;
};
typedef std::vector<TilePartLengthInfo> TL_INFO_VEC;
// map of (TLM marker id) => (tile part length vector)
typedef std::map<uint8_t, TL_INFO_VEC*> TL_MAP;

struct TileLengthMarkers
{
	TileLengthMarkers();
	TileLengthMarkers(IBufferedStream* stream);
	~TileLengthMarkers();

	bool read(uint8_t* headerData, uint16_t header_size);
	void rewind(void);
	TilePartLengthInfo getNext(void);
	bool skipTo(uint16_t skipTileIndex, IBufferedStream* stream, uint64_t firstSotPos);

	bool writeBegin(uint16_t numTilePartsTotal);
	void push(uint16_t tileIndex, uint32_t tile_part_size);
	bool writeEnd(void);
	/**
	 Add tile header marker information
	 @param tileno       tile index number
	 @param codeStreamInfo   Codestream information structure
	 @param type         marker type
	 @param pos          byte offset of marker segment
	 @param len          length of marker segment
	 */
	static bool addTileMarkerInfo(uint16_t tileno, CodeStreamInfo* codeStreamInfo, uint16_t type,
								  uint64_t pos, uint32_t len);

  private:
	void push(uint8_t i_TLM, TilePartLengthInfo curr_vec);
	TL_MAP* m_markers;
	uint8_t m_markerIndex;
	uint8_t m_markerTilePartIndex;
	TL_INFO_VEC* m_curr_vec;
	IBufferedStream* m_stream;
	uint64_t streamStart;
};

struct PacketInfo
{
	PacketInfo(void);
	uint32_t getPacketDataLength(void);
	uint32_t headerLength;
	uint32_t packetLength;
	bool parsedData;
};

struct PacketInfoCache
{
	PacketInfoCache();
	~PacketInfoCache();

	std::vector<PacketInfo*> packetInfo;
};

} // namespace grk
