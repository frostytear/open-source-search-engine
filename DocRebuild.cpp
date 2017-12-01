//
// Copyright (C) 2017 Privacore ApS - https://www.privacore.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// License TL;DR: If you change this file, you must publish your changes.
//
#include "DocRebuild.h"
#include "XmlDoc.h"
#include "Msg0.h"
#include "RdbList.h"

DocRebuild g_docRebuild("docrebuild.txt", false);
DocRebuild g_docRebuildUrl("docrebuildurl.txt", true);

struct DocRebuildDocItem : public DocProcessDocItem {
	DocRebuildDocItem(DocProcess *docProcess, const std::string &key, int64_t lastPos)
		: DocProcessDocItem(docProcess, key, lastPos)
		  , m_msg0()
		  , m_spiderdbList()
		  , m_spiderdbListRequested(false)
		  , m_spiderdbListProcessed(false) {
	}

	Msg0 m_msg0;
	RdbList m_spiderdbList;
	bool m_spiderdbListRequested;
	bool m_spiderdbListProcessed;
};

DocRebuild::DocRebuild(const char *filename, bool isUrl)
	: DocProcess(filename, isUrl) {
}

DocProcessDocItem* DocRebuild::createDocItem(DocProcess *docProcess, const std::string &key, int64_t lastPos) {
	return new DocRebuildDocItem(docProcess, key, lastPos);
}

void DocRebuild::updateXmldoc(XmlDoc *xmlDoc) {
	xmlDoc->m_recycleContent = true;
}

void DocRebuild::processDocItem(DocProcessDocItem *docItem) {
	DocRebuildDocItem *rebuildDocItem = dynamic_cast<DocRebuildDocItem*>(docItem);
	XmlDoc *xmlDoc = rebuildDocItem->m_xmlDoc;

	// set callback
	xmlDoc->m_masterLoop = processedDoc;
	xmlDoc->m_masterState = rebuildDocItem;

	// prepare
	char **oldTitleRec = xmlDoc->getOldTitleRec();
	if (!oldTitleRec || oldTitleRec == (char**)-1) {
		return;
	}

	// oldTitleRec is mandatory for docrebuild
	if (*oldTitleRec == nullptr) {
		xmlDoc->m_indexCode = ENOTFOUND;
		xmlDoc->m_indexCodeValid = true;

		xmlDoc->logIt();

		removePendingDoc(rebuildDocItem);

		delete xmlDoc;
		delete rebuildDocItem;

		return;
	}

	// reset callback
	xmlDoc->m_masterLoop = nullptr;
	xmlDoc->m_masterState = nullptr;

	if (!xmlDoc->set2(*oldTitleRec, -1, "main", nullptr, MAX_NICENESS)) {
		xmlDoc->m_indexCode = ECORRUPTDATA;
		xmlDoc->m_indexCodeValid = true;

		xmlDoc->logIt();

		removePendingDoc(rebuildDocItem);

		delete xmlDoc;
		delete rebuildDocItem;

		return;
	}

	int32_t *firstIp = xmlDoc->getFirstIp();
	if (!firstIp || firstIp == (int32_t*)-1) {
		// we must not be blocked/invalid at this point
		gbshutdownLogicError();
	}

	int32_t *siteNumInLinks = xmlDoc->getSiteNumInlinks();
	if (!siteNumInLinks || siteNumInLinks == (int32_t*)-1) {
		// blocked
		return;
	}

	// reset callback
	if (xmlDoc->m_masterLoop == processedDoc) {
		xmlDoc->m_masterLoop = nullptr;
		xmlDoc->m_masterState = nullptr;
	}

	// set spider request
	if (!rebuildDocItem->m_spiderdbListRequested) {
		int64_t urlHash48 = xmlDoc->getFirstUrlHash48();
		key128_t startKey = Spiderdb::makeKey(*firstIp, urlHash48, true, 0, false);
		key128_t endKey = Spiderdb::makeKey(*firstIp, urlHash48, true, MAX_DOCID, false);

		rebuildDocItem->m_spiderdbListRequested = true;

		if (!rebuildDocItem->m_msg0.getList(-1, RDB_SPIDERDB, xmlDoc->m_collnum, &rebuildDocItem->m_spiderdbList, (const char *)&startKey,
		                             (const char *)&endKey,
		                             1000000, rebuildDocItem, processedDoc, 0, true, true, -1, 0, -1, 10000, false, false, -1)) {
			// blocked
			return;
		}
	}

	if (!rebuildDocItem->m_spiderdbListProcessed) {
		if (rebuildDocItem->m_spiderdbList.isEmpty()) {
			xmlDoc->getRebuiltSpiderRequest(&xmlDoc->m_sreq);
			xmlDoc->m_addSpiderRequest = true;
		} else {
			SpiderRequest *sreq = reinterpret_cast<SpiderRequest *>(rebuildDocItem->m_spiderdbList.getCurrentRec());
			xmlDoc->m_sreq = *sreq;
		}

		xmlDoc->m_sreqValid = true;
		rebuildDocItem->m_spiderdbListProcessed = true;
	}

	// done
	if (xmlDoc->m_indexedDoc || xmlDoc->indexDoc()) {
		removePendingDoc(rebuildDocItem);

		delete xmlDoc;
		delete rebuildDocItem;
	}
}