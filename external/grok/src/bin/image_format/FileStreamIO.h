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
 *
 *
 */

#pragma once

#include "IFileIO.h"

class FileStreamIO : public IFileIO
{
  public:
	FileStreamIO();
	virtual ~FileStreamIO() override;
	bool open(std::string fileName, std::string mode) override;
	bool close(void) override;
	bool write(uint8_t* buf, size_t len) override;
	bool read(uint8_t* buf, size_t len) override;
	bool seek(int64_t pos) override;
	FILE* getFileStream()
	{
		return m_fileHandle;
	}

  private:
	FILE* m_fileHandle;
	std::string m_fileName;
};
