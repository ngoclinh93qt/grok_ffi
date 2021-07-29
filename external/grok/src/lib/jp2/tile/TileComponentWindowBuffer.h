/**
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
 *
 */
#pragma once

#include "grk_includes.h"
#include <stdexcept>
#include <algorithm>

namespace grk
{
/*
 Various coordinate systems are used to describe regions in the tile component buffer.

 1) Canvas coordinates:  JPEG 2000 global image coordinates. For tile component, sub-sampling is
 applied.

 2) Band coordinates: coordinates relative to a specified sub-band's origin

 3) Buffer coordinates: coordinate system where all resolutions are translated
	to common origin (0,0). If each code block is translated relative to the origin of the
 resolution that **it belongs to**, the blocks are then all in buffer coordinate system

 Note: the name of any method or variable returning non canvas coordinates is appended
 with "REL", to signify relative coordinates.

 */

enum eSplitOrientation
{
	SPLIT_L,
	SPLIT_H,
	SPLIT_NUM_ORIENTATIONS
};

/**
 *  Class to manage multiple buffers needed to perform DWT transform
 *
 */
template<typename T>
struct BufferResWindow
{
	BufferResWindow(uint8_t numresolutions, uint8_t resno,
					grkBuffer2d<T, AllocatorAligned>* resWindowTopLevelREL,
					Resolution* tileCompAtRes, Resolution* tileCompAtLowerRes,
					grkRectU32 tileCompWindow, grkRectU32 tileCompWindowUnreduced,
					grkRectU32 tileCompUnreduced, uint32_t FILTER_WIDTH)
		: m_allocated(false), m_tileCompRes(tileCompAtRes), m_tileCompResLower(tileCompAtLowerRes),
		  m_bufferResWindowBufferREL(new grkBuffer2d<T, AllocatorAligned>(tileCompWindow.width(),
																		  tileCompWindow.height())),
		  m_bufferResWindowTopLevelBufferREL(resWindowTopLevelREL), m_filterWidth(FILTER_WIDTH)
	{
		for(uint32_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
			m_splitResWindowBufferREL[i] = nullptr;
		// windowed decompression
		if(FILTER_WIDTH)
		{
			uint32_t numDecomps =
				(resno == 0) ? (uint32_t)(numresolutions - 1U) : (uint32_t)(numresolutions - resno);

			/*
			m_paddedBandWindow is only used for determining which precincts and code blocks overlap
			the window of interest, in each respective resolution
			*/
			for(uint8_t orient = 0; orient < ((resno) > 0 ? BAND_NUM_ORIENTATIONS : 1); orient++)
			{
				m_paddedBandWindow.push_back(
					getTileCompBandWindow(numDecomps, orient, tileCompWindowUnreduced,
										  tileCompUnreduced, 2 * FILTER_WIDTH));
			}

			if(m_tileCompResLower)
			{
				assert(resno > 0);
				for(uint8_t orient = 0; orient < BAND_NUM_ORIENTATIONS; orient++)
				{
					// todo: should only need padding equal to FILTER_WIDTH, not 2*FILTER_WIDTH
					auto tileBandWindow =
						getTileCompBandWindow(numDecomps, orient, tileCompWindowUnreduced,
											  tileCompUnreduced, 2 * FILTER_WIDTH);
					auto tileBand = orient == BAND_ORIENT_LL ? *((grkRectU32*)m_tileCompResLower)
															 : m_tileCompRes->tileBand[orient - 1];
					auto bandWindowREL =
						tileBandWindow.pan(-(int64_t)tileBand.x0, -(int64_t)tileBand.y0);
					m_paddedBandWindowBufferREL.push_back(
						new grkBuffer2d<T, AllocatorAligned>(&bandWindowREL));
				}
				auto winLow = m_paddedBandWindowBufferREL[BAND_ORIENT_LL];
				auto winHigh = m_paddedBandWindowBufferREL[BAND_ORIENT_HL];
				m_bufferResWindowBufferREL->x0 = (std::min<uint32_t>)(2 * winLow->x0, 2 * winHigh->x0 + 1);
				m_bufferResWindowBufferREL->x1 = (std::max<uint32_t>)(2 * winLow->x1, 2 * winHigh->x1 + 1);
				winLow = m_paddedBandWindowBufferREL[BAND_ORIENT_LL];
				winHigh = m_paddedBandWindowBufferREL[BAND_ORIENT_LH];
				m_bufferResWindowBufferREL->y0 = (std::min<uint32_t>)(2 * winLow->y0, 2 * winHigh->y0 + 1);
				m_bufferResWindowBufferREL->y1 = (std::max<uint32_t>)(2 * winLow->y1, 2 * winHigh->y1 + 1);

				// todo: shouldn't need to clip
				auto resBounds = grkRectU32(0, 0, m_tileCompRes->width(), m_tileCompRes->height());
				m_bufferResWindowBufferREL->clip(&resBounds);

				// two windows formed by horizontal pass and used as input for vertical pass
				grkRectU32 splitResWindowREL[SPLIT_NUM_ORIENTATIONS];
				splitResWindowREL[SPLIT_L] = grkRectU32(
					m_bufferResWindowBufferREL->x0, m_paddedBandWindowBufferREL[BAND_ORIENT_LL]->y0,
					m_bufferResWindowBufferREL->x1,
					m_paddedBandWindowBufferREL[BAND_ORIENT_LL]->y1);
				m_splitResWindowBufferREL[SPLIT_L] =
					new grkBuffer2d<T, AllocatorAligned>(&splitResWindowREL[SPLIT_L]);
				splitResWindowREL[SPLIT_H] = grkRectU32(
					m_bufferResWindowBufferREL->x0,
					m_paddedBandWindowBufferREL[BAND_ORIENT_LH]->y0 + m_tileCompResLower->height(),
					m_bufferResWindowBufferREL->x1,
					m_paddedBandWindowBufferREL[BAND_ORIENT_LH]->y1 + m_tileCompResLower->height());
				m_splitResWindowBufferREL[SPLIT_H] =
					new grkBuffer2d<T, AllocatorAligned>(&splitResWindowREL[SPLIT_H]);
			}
			// compression or full tile decompression
		}
		else
		{
			// dummy LL band window
			m_paddedBandWindowBufferREL.push_back(new grkBuffer2d<T, AllocatorAligned>(0, 0));
			assert(tileCompAtRes->numTileBandWindows == 3 || !tileCompAtLowerRes);
			if(m_tileCompResLower)
			{
				for(uint32_t i = 0; i < tileCompAtRes->numTileBandWindows; ++i)
				{
					auto b = tileCompAtRes->tileBand + i;
					m_paddedBandWindowBufferREL.push_back(
						new grkBuffer2d<T, AllocatorAligned>(b->width(), b->height()));
				}
				// note: only dimensions of split resolution window buffer matter, not actual
				// coordinates
				for(uint32_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
					m_splitResWindowBufferREL[i] = new grkBuffer2d<T, AllocatorAligned>(
						tileCompWindow.width(), tileCompWindow.height() / 2);
			}
		}
	}
	~BufferResWindow()
	{
		delete m_bufferResWindowBufferREL;
		for(auto& b : m_paddedBandWindowBufferREL)
			delete b;
		for(uint32_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
			delete m_splitResWindowBufferREL[i];
	}
	bool alloc(bool clear)
	{
		if(m_allocated)
			return true;

		// if top level window is present, then all buffers attach to this window
		if(m_bufferResWindowTopLevelBufferREL)
		{
			// ensure that top level window is allocated
			if(!m_bufferResWindowTopLevelBufferREL->alloc2d(clear))
				return false;

			// for now, we don't allocate bandWindows for windowed decompression
			if(m_filterWidth)
				return true;

			// attach to top level window
			if(m_bufferResWindowBufferREL != m_bufferResWindowTopLevelBufferREL)
				m_bufferResWindowBufferREL->attach(m_bufferResWindowTopLevelBufferREL->getBuffer(),
												   m_bufferResWindowTopLevelBufferREL->stride);

			// m_tileCompResLower is null for lowest resolution
			if(m_tileCompResLower)
			{
				for(uint8_t orientation = 0; orientation < m_paddedBandWindowBufferREL.size();
					++orientation)
				{
					switch(orientation)
					{
						case BAND_ORIENT_HL:
							m_paddedBandWindowBufferREL[orientation]->attach(
								m_bufferResWindowTopLevelBufferREL->getBuffer() +
									m_tileCompResLower->width(),
								m_bufferResWindowTopLevelBufferREL->stride);
							break;
						case BAND_ORIENT_LH:
							m_paddedBandWindowBufferREL[orientation]->attach(
								m_bufferResWindowTopLevelBufferREL->getBuffer() +
									m_tileCompResLower->height() *
										m_bufferResWindowTopLevelBufferREL->stride,
								m_bufferResWindowTopLevelBufferREL->stride);
							break;
						case BAND_ORIENT_HH:
							m_paddedBandWindowBufferREL[orientation]->attach(
								m_bufferResWindowTopLevelBufferREL->getBuffer() +
									m_tileCompResLower->width() +
									m_tileCompResLower->height() *
										m_bufferResWindowTopLevelBufferREL->stride,
								m_bufferResWindowTopLevelBufferREL->stride);
							break;
						default:
							break;
					}
				}
				m_splitResWindowBufferREL[SPLIT_L]->attach(
					m_bufferResWindowTopLevelBufferREL->getBuffer(),
					m_bufferResWindowTopLevelBufferREL->stride);
				m_splitResWindowBufferREL[SPLIT_H]->attach(
					m_bufferResWindowTopLevelBufferREL->getBuffer() +
						m_tileCompResLower->height() * m_bufferResWindowTopLevelBufferREL->stride,
					m_bufferResWindowTopLevelBufferREL->stride);
			}
		}
		else
		{
			// resolution window is always allocated
			if(!m_bufferResWindowBufferREL->alloc2d(clear))
				return false;
			// for now, we don't allocate bandWindows for windowed decode
			if(m_filterWidth)
				return true;

			// band windows are allocated if present
			for(auto& b : m_paddedBandWindowBufferREL)
			{
				if(!b->alloc2d(clear))
					return false;
			}
			if(m_tileCompResLower)
			{
				m_splitResWindowBufferREL[SPLIT_L]->attach(m_bufferResWindowBufferREL->getBuffer(),
														   m_bufferResWindowBufferREL->stride);
				m_splitResWindowBufferREL[SPLIT_H]->attach(
					m_bufferResWindowBufferREL->getBuffer() +
						m_tileCompResLower->height() * m_bufferResWindowBufferREL->stride,
					m_bufferResWindowBufferREL->stride);
			}
		}
		m_allocated = true;

		return true;
	}

	/**
	 * Get band window in tile component coordinates for specified number
	 * of decompositions
	 *
	 * Note: if num_res is zero, then the band window (and there is only one)
	 * is equal to the unreduced tile component window
	 *
	 * Compute number of decomposition for this band. See table F-1
	 * uint32_t numDecomps = (resno == 0) ?
	 *		(uint32_t)(numresolutions - 1U) : (uint32_t)(numresolutions - resno);
	 *
	 */
	static grkRectU32 getTileCompBandWindow(uint32_t numDecomps, uint8_t orientation,
											grkRectU32 unreducedTileCompWindow)
	{
		assert(orientation < BAND_NUM_ORIENTATIONS);
		if(numDecomps == 0)
			return unreducedTileCompWindow;

		uint32_t tcx0 = unreducedTileCompWindow.x0;
		uint32_t tcy0 = unreducedTileCompWindow.y0;
		uint32_t tcx1 = unreducedTileCompWindow.x1;
		uint32_t tcy1 = unreducedTileCompWindow.y1;

		/* Map above tile-based coordinates to
		 * sub-band-based coordinates, i.e. origin is at tile origin  */
		/* See equation B-15 of the standard. */
		uint32_t bx0 = orientation & 1;
		uint32_t by0 = (uint32_t)(orientation >> 1U);

		uint32_t bx0Shift = (1U << (numDecomps - 1)) * bx0;
		uint32_t by0Shift = (1U << (numDecomps - 1)) * by0;

		return grkRectU32(
			(tcx0 <= bx0Shift) ? 0 : ceildivpow2<uint32_t>(tcx0 - bx0Shift, numDecomps),
			(tcy0 <= by0Shift) ? 0 : ceildivpow2<uint32_t>(tcy0 - by0Shift, numDecomps),
			(tcx1 <= bx0Shift) ? 0 : ceildivpow2<uint32_t>(tcx1 - bx0Shift, numDecomps),
			(tcy1 <= by0Shift) ? 0 : ceildivpow2<uint32_t>(tcy1 - by0Shift, numDecomps));
	}
	/**
	 * Get band window in tile component coordinates for specified number
	 * of decompositions (with padding)
	 *
	 * Note: if num_res is zero, then the band window (and there is only one)
	 * is equal to the unreduced tile component window (with padding)
	 */
	static grkRectU32 getTileCompBandWindow(uint32_t numDecomps, uint8_t orientation,
											grkRectU32 unreducedTileCompWindow,
											grkRectU32 unreducedTileComp, uint32_t padding)
	{
		assert(orientation < BAND_NUM_ORIENTATIONS);
		if(numDecomps == 0)
		{
			assert(orientation == 0);
			return unreducedTileCompWindow.grow(padding).intersection(&unreducedTileComp);
		}
		auto oneLessDecompWindow = unreducedTileCompWindow;
		auto oneLessDecompTile = unreducedTileComp;
		if(numDecomps > 1)
		{
			oneLessDecompWindow = getTileCompBandWindow(numDecomps - 1, 0, unreducedTileCompWindow);
			oneLessDecompTile = getTileCompBandWindow(numDecomps - 1, 0, unreducedTileComp);
		}

		return getTileCompBandWindow(
			1, orientation, oneLessDecompWindow.grow(2 * padding).intersection(&oneLessDecompTile));
	}
	bool m_allocated;
	Resolution* m_tileCompRes; // when non-null, it triggers creation of band window buffers
	Resolution* m_tileCompResLower; // null for lowest resolution
	std::vector<grkBuffer2d<T, AllocatorAligned>*>
		m_paddedBandWindowBufferREL; // coordinates are relative to band origin
	std::vector<grkRectU32> m_paddedBandWindow; // canvas coordinates
	grkBuffer2d<T, AllocatorAligned>*
		m_splitResWindowBufferREL[SPLIT_NUM_ORIENTATIONS]; // buffer coordinates
	grkBuffer2d<T, AllocatorAligned>* m_bufferResWindowBufferREL; // buffer coordinates
	grkBuffer2d<T, AllocatorAligned>* m_bufferResWindowTopLevelBufferREL; // buffer coordinates
	uint32_t m_filterWidth;
};
template<typename T>
struct TileComponentWindowBuffer
{
	TileComponentWindowBuffer(bool isCompressor, bool lossless, bool wholeTileDecompress,
							  grkRectU32 tileCompUnreduced, grkRectU32 tileCompReduced,
							  grkRectU32 unreducedTileCompOrImageCompWindow,
							  Resolution* tileCompResolution, uint8_t numresolutions,
							  uint8_t reducedNumResolutions)
		: m_unreducedBounds(tileCompUnreduced), m_bounds(tileCompReduced),
		  m_numResolutions(numresolutions), m_compress(isCompressor),
		  m_wholeTileDecompress(wholeTileDecompress)
	{
		if(!m_compress)
		{
			// for decompress, we are passed the unreduced image component window
			auto unreducedImageCompWindow = unreducedTileCompOrImageCompWindow;
			m_bounds = unreducedImageCompWindow.rectceildivpow2(
				(uint32_t)(m_numResolutions - reducedNumResolutions));
			m_bounds = m_bounds.intersection(tileCompReduced);
			assert(m_bounds.is_valid());
			m_unreducedBounds = unreducedImageCompWindow.intersection(tileCompUnreduced);
			assert(m_unreducedBounds.is_valid());
		}
		// fill resolutions vector
		assert(reducedNumResolutions > 0);
		for(uint32_t resno = 0; resno < reducedNumResolutions; ++resno)
			m_resolution.push_back(tileCompResolution + resno);

		auto tileCompAtRes = tileCompResolution + reducedNumResolutions - 1;
		auto tileCompAtLowerRes =
			reducedNumResolutions > 1 ? tileCompResolution + reducedNumResolutions - 2 : nullptr;
		// create resolution buffers
		auto topLevel = new BufferResWindow<T>(
			numresolutions, (uint8_t)(reducedNumResolutions - 1U), nullptr, tileCompAtRes,
			tileCompAtLowerRes, m_bounds, m_unreducedBounds, tileCompUnreduced,
			wholeTileDecompress ? 0 : getFilterPad<uint32_t>(lossless));
		// setting top level prevents allocation of tileCompBandWindows buffers
		if(!useBandWindows())
			topLevel->m_bufferResWindowTopLevelBufferREL = topLevel->m_bufferResWindowBufferREL;

		for(uint8_t resno = 0; resno < reducedNumResolutions - 1; ++resno)
		{
			// resolution window ==  next resolution band window at orientation 0
			auto resDims = BufferResWindow<T>::getTileCompBandWindow(
				(uint32_t)(numresolutions - 1 - resno), 0, m_unreducedBounds);
			m_bufferResWindowREL.push_back(new BufferResWindow<T>(
				numresolutions, resno,
				useBandWindows() ? nullptr : topLevel->m_bufferResWindowBufferREL,
				tileCompResolution + resno, resno > 0 ? tileCompResolution + resno - 1 : nullptr,
				resDims, m_unreducedBounds, tileCompUnreduced,
				wholeTileDecompress ? 0 : getFilterPad<uint32_t>(lossless)));
		}
		m_bufferResWindowREL.push_back(topLevel);
	}
	~TileComponentWindowBuffer()
	{
		for(auto& b : m_bufferResWindowREL)
			delete b;
	}

	/**
	 * Transform code block offsets from canvas coordinates
	 * to either band coordinates (relative to sub band origin)
	 * or buffer coordinates (relative to associated resolution origin)
	 *
	 * @param resno resolution number
	 * @param orientation band orientation {LL,HL,LH,HH}
	 * @param offsetx x offset of code block in canvas coordinates
	 * @param offsety y offset of code block in canvas coordinates
	 *
	 */
	void toRelativeCoordinates(uint8_t resno, eBandOrientation orientation, uint32_t& offsetx,
							   uint32_t& offsety) const
	{
		assert(resno < m_resolution.size());

		auto res = m_resolution[resno];
		auto band = res->tileBand + getBandIndex(resno, orientation);

		uint32_t x = offsetx;
		uint32_t y = offsety;

		// get offset relative to band
		x -= band->x0;
		y -= band->y0;

		if(useBufferCoordinatesForCodeblock() && resno > 0)
		{
			auto resLower = m_resolution[resno - 1U];

			if(orientation & 1)
				x += resLower->width();
			if(orientation & 2)
				y += resLower->height();
		}
		offsetx = x;
		offsety = y;
	}
	/**
	 * Get code block destination window
	 *
	 * @param resno resolution number
	 * @param orientation band orientation {LL,HL,LH,HH}
	 *
	 */
	const grkBuffer2d<T, AllocatorAligned>*
		getCodeBlockDestWindowREL(uint8_t resno, eBandOrientation orientation) const
	{
		return (useBufferCoordinatesForCodeblock()) ? getHighestBufferResWindowREL()
													: getBandWindowREL(resno, orientation);
	}
	/**
	 * Get band window, in relative band coordinates
	 *
	 * @param resno resolution number
	 * @param orientation band orientation {0,1,2,3} for {LL,HL,LH,HH} band windows
	 *
	 * If resno is > 0, return LL,HL,LH or HH band window, otherwise return LL resolution window
	 *
	 */
	const grkBuffer2d<T, AllocatorAligned>* getBandWindowREL(uint8_t resno,
															 eBandOrientation orientation) const
	{
		assert(resno < m_resolution.size());
		assert(resno > 0 || orientation == BAND_ORIENT_LL);

		if(resno == 0 && (m_compress || m_wholeTileDecompress))
			return m_bufferResWindowREL[0]->m_bufferResWindowBufferREL;

		return m_bufferResWindowREL[resno]->m_paddedBandWindowBufferREL[orientation];
	}
	/**
	 * Get padded band window, in canvas coordinates
	 *
	 * @param resno resolution number
	 * @param orientation band orientation {0,1,2,3} for {LL,HL,LH,HH} band windows
	 *
	 */
	const grkRectU32* getPaddedBandWindow(uint8_t resno, eBandOrientation orientation) const
	{
		if(m_bufferResWindowREL[resno]->m_paddedBandWindow.empty())
			return nullptr;
		return &m_bufferResWindowREL[resno]->m_paddedBandWindow[orientation];
	}
	/*
	 * Get intermediate split window, in buffer coordinates
	 *
	 * @param orientation 0 for upper split window, and 1 for lower split window
	 */
	const grkBuffer2d<T, AllocatorAligned>* getSplitWindowREL(uint8_t resno,
															  eSplitOrientation orientation) const
	{
		assert(resno > 0 && resno < m_resolution.size());

		return m_bufferResWindowREL[resno]->m_splitResWindowBufferREL[orientation];
	}
	/**
	 * Get resolution window, in buffer coordinates
	 *
	 * @param resno resolution number
	 *
	 */
	const grkBuffer2d<T, AllocatorAligned>* getBufferResWindowREL(uint32_t resno) const
	{
		return m_bufferResWindowREL[resno]->m_bufferResWindowBufferREL;
	}
	/**
	 * Get highest resolution window, in buffer coordinates
	 *
	 *
	 */
	grkBuffer2d<T, AllocatorAligned>* getHighestBufferResWindowREL(void) const
	{
		return m_bufferResWindowREL.back()->m_bufferResWindowBufferREL;
	}
	bool alloc()
	{
		for(auto& b : m_bufferResWindowREL)
		{
			if(!b->alloc(!m_compress))
				return false;
		}

		return true;
	}
	/**
	 * Get bounds of tile component (canvas coordinates)
	 * decompress: reduced canvas coordinates of window
	 * compress: unreduced canvas coordinates of entire tile
	 */
	grkRectU32 bounds() const
	{
		return m_bounds;
	}
	grkRectU32 unreducedBounds() const
	{
		return m_unreducedBounds;
	}
	uint64_t stridedArea(void) const
	{
		return getHighestBufferResWindowREL()->stride * m_bounds.height();
	}

	// set data to buf without owning it
	void attach(T* buffer, uint32_t stride)
	{
		getHighestBufferResWindowREL()->attach(buffer, stride);
	}
	// transfer data to buf, and cease owning it
	void transfer(T** buffer, uint32_t* stride)
	{
		getHighestBufferResWindowREL()->transfer(buffer, stride);
	}

  private:
	bool useBandWindows() const
	{
		// return !m_compress && m_wholeTileDecompress;
		return false;
	}
	bool useBufferCoordinatesForCodeblock() const
	{
		return m_compress || !m_wholeTileDecompress;
	}
	uint8_t getBandIndex(uint8_t resno, eBandOrientation orientation) const
	{
		uint8_t index = 0;
		if(resno > 0)
		{
			index = (uint8_t)orientation;
			index--;
		}
		return index;
	}
	/******************************************************/
	// decompress: unreduced/reduced image component window
	// compress:  unreduced/reduced tile component
	grkRectU32 m_unreducedBounds;
	grkRectU32 m_bounds;
	/******************************************************/
	std::vector<Resolution*> m_resolution;
	// windowed bounds for windowed decompress, otherwise full bounds
	std::vector<BufferResWindow<T>*> m_bufferResWindowREL;
	// unreduced number of resolutions
	uint8_t m_numResolutions;
	bool m_compress;
	bool m_wholeTileDecompress;
};

} // namespace grk
