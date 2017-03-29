// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#ifndef  VIVALDI_IMPORTER_VIV_OPERA_READER_H_
#define VIVALDI_IMPORTER_VIV_OPERA_READER_H_

#include "base/values.h"

class OperaAdrFileReader 
{
public:
	void LoadFile(base::FilePath &file);

protected:
	virtual void HandleEntry(const std::string &category, const base::DictionaryValue &entries) = 0;

	OperaAdrFileReader();
	virtual ~OperaAdrFileReader();

private:

DISALLOW_COPY_AND_ASSIGN(OperaAdrFileReader);
};




#endif // VIVALDI_IMPORTER_VIV_OPERA_READER_H_
