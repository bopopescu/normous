/**
 * Copyright 2011 (c) 10gen Inc.
 *
 * This program is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pch.h"

#include "mongo/db/pipeline/document_source.h"
#include "mongo/s/shard.h"

namespace mongo {

    DocumentSourceCommandShards::~DocumentSourceCommandShards() {
    }

    bool DocumentSourceCommandShards::eof() {
        /* if we haven't even started yet, do so */
        if (!pCurrent.get())
            getNextDocument();

        return (pCurrent.get() == NULL);
    }

    bool DocumentSourceCommandShards::advance() {
        DocumentSource::advance(); // check for interrupts

        if (eof())
            return false;

        /* advance */
        getNextDocument();

        return (pCurrent.get() != NULL);
    }

    intrusive_ptr<Document> DocumentSourceCommandShards::getCurrent() {
        verify(!eof());
        return pCurrent;
    }

    void DocumentSourceCommandShards::setSource(DocumentSource *pSource) {
        /* this doesn't take a source */
        verify(false);
    }

    void DocumentSourceCommandShards::sourceToBson(
        BSONObjBuilder *pBuilder, bool explain) const {
        /* this has no BSON equivalent */
        verify(false);
    }

    DocumentSourceCommandShards::DocumentSourceCommandShards(
        const ShardOutput& shardOutput,
        const intrusive_ptr<ExpressionContext> &pExpCtx):
        DocumentSource(pExpCtx),
        newSource(false),
        pBsonSource(),
        pCurrent(),
        iterator(shardOutput.begin()),
        listEnd(shardOutput.end())
    {}

    intrusive_ptr<DocumentSourceCommandShards>
    DocumentSourceCommandShards::create(
        const ShardOutput& shardOutput,
        const intrusive_ptr<ExpressionContext> &pExpCtx) {
        intrusive_ptr<DocumentSourceCommandShards> pSource(
            new DocumentSourceCommandShards(shardOutput, pExpCtx));
        return pSource;
    }

    void DocumentSourceCommandShards::getNextDocument() {
        while(true) {
            if (!pBsonSource.get()) {
                /* if there aren't any more futures, we're done */
                if (iterator == listEnd) {
                    pCurrent.reset();
                    return;
                }

                /* grab the next command result */
                BSONObj resultObj = iterator->second;

                uassert(16387, str::stream() << "sharded pipeline failed on shard " <<
                                            iterator->first.getName() << ": " <<
                                            resultObj.toString(),
                        resultObj["ok"].trueValue());

                /* grab the result array out of the shard server's response */
                BSONElement resultArray = resultObj["result"];
                massert(16388, str::stream() << "no result array? shard:" <<
                                            iterator->first.getName() << ": " <<
                                            resultObj.toString(),
                        resultArray.type() == Array);

                // done with error checking, don't need the shard name anymore
                ++iterator;

                if (resultArray.embeddedObject().isEmpty()){
                    // this shard had no results, on to the next one
                    continue;
                }

                pBsonSource = DocumentSourceBsonArray::create(&resultArray, pExpCtx);
                newSource = true;
            }

            /* if we're done with this shard's results, try the next */
            if (pBsonSource->eof() ||
                (!newSource && !pBsonSource->advance())) {
                pBsonSource.reset();
                continue;
            }

            pCurrent = pBsonSource->getCurrent();
            newSource = false;
            return;
        }
    }
}
