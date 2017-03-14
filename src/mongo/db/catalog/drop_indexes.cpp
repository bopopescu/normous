/**
 *    Copyright (C) 2015 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kCommand

#include "mongo/platform/basic.h"

#include "mongo/db/catalog/drop_indexes.h"

#include "mongo/db/background.h"
#include "mongo/db/catalog/collection.h"
#include "mongo/db/catalog/database.h"
#include "mongo/db/catalog/index_catalog.h"
#include "mongo/db/client.h"
#include "mongo/db/concurrency/write_conflict_exception.h"
#include "mongo/db/curop.h"
#include "mongo/db/db_raii.h"
#include "mongo/db/index/index_descriptor.h"
#include "mongo/db/index_builder.h"
#include "mongo/db/repl/replication_coordinator_global.h"
#include "mongo/db/service_context.h"
#include "mongo/util/log.h"

namespace mongo {
namespace {
Status wrappedRun(OperationContext* opCtx,
                  const NamespaceString& nss,
                  Database* const db,
                  const BSONObj& jsobj,
                  BSONObjBuilder* anObjBuilder) {
    if (!serverGlobalParams.quiet.load()) {
        LOG(0) << "CMD: dropIndexes " << nss;
    }
    Collection* collection = db ? db->getCollection(nss) : nullptr;

    // If db/collection does not exist, short circuit and return.
    if (!db || !collection) {
        if (db && db->getViewCatalog()->lookup(opCtx, nss.ns())) {
            return {ErrorCodes::CommandNotSupportedOnView,
                    str::stream() << "Cannot drop indexes on view " << nss.ns()};
        }

        return Status(ErrorCodes::NamespaceNotFound, "ns not found");
    }

    OldClientContext ctx(opCtx, nss.ns());
    BackgroundOperation::assertNoBgOpInProgForNs(nss);

    IndexCatalog* indexCatalog = collection->getIndexCatalog();
    anObjBuilder->appendNumber("nIndexesWas", indexCatalog->numIndexesTotal(opCtx));

    BSONElement f = jsobj.getField("index");
    if (f.type() == String) {
        std::string indexToDelete = f.valuestr();

        if (indexToDelete == "*") {
            std::map<std::string, BSONObj> droppedIndexes;
            Status s = indexCatalog->dropAllIndexes(opCtx, false, &droppedIndexes);
            if (!s.isOK()) {
                return s;
            }

            // We log one op for every dropped index so that we can roll them back if necessary.
            for (auto const& idx : droppedIndexes) {
                opCtx->getServiceContext()->getOpObserver()->onDropIndex(
                    opCtx, nss, idx.first, idx.second);
            }

            anObjBuilder->append("msg", "non-_id indexes dropped for collection");
            return Status::OK();
        }

        IndexDescriptor* desc =
            collection->getIndexCatalog()->findIndexByName(opCtx, indexToDelete);
        if (desc == NULL) {
            return Status(ErrorCodes::IndexNotFound,
                          str::stream() << "index not found with name [" << indexToDelete << "]");
        }

        if (desc->isIdIndex()) {
            return Status(ErrorCodes::InvalidOptions, "cannot drop _id index");
        }

        Status s = indexCatalog->dropIndex(opCtx, desc);
        if (!s.isOK()) {
            return s;
        }

        opCtx->getServiceContext()->getOpObserver()->onDropIndex(
            opCtx, nss, desc->indexName(), desc->infoObj());

        return Status::OK();
    }

    if (f.type() == Object) {
        std::vector<IndexDescriptor*> indexes;
        collection->getIndexCatalog()->findIndexesByKeyPattern(
            opCtx, f.embeddedObject(), false, &indexes);
        if (indexes.empty()) {
            return Status(ErrorCodes::IndexNotFound,
                          str::stream() << "can't find index with key: " << f.embeddedObject());
        } else if (indexes.size() > 1) {
            return Status(ErrorCodes::AmbiguousIndexKeyPattern,
                          str::stream() << indexes.size() << " indexes found for key: "
                                        << f.embeddedObject()
                                        << ", identify by name instead."
                                        << " Conflicting indexes: "
                                        << indexes[0]->infoObj()
                                        << ", "
                                        << indexes[1]->infoObj());
        }

        IndexDescriptor* desc = indexes[0];
        if (desc->isIdIndex()) {
            return Status(ErrorCodes::InvalidOptions, "cannot drop _id index");
        }

        Status s = indexCatalog->dropIndex(opCtx, desc);
        if (!s.isOK()) {
            return s;
        }

        opCtx->getServiceContext()->getOpObserver()->onDropIndex(
            opCtx, nss, desc->indexName(), desc->infoObj());

        return Status::OK();
    }

    return Status(ErrorCodes::IndexNotFound, "invalid index name spec");
}
}  // namespace

Status dropIndexes(OperationContext* opCtx,
                   const NamespaceString& nss,
                   const BSONObj& cmdObj,
                   BSONObjBuilder* result) {
    MONGO_WRITE_CONFLICT_RETRY_LOOP_BEGIN {
        ScopedTransaction transaction(opCtx, MODE_IX);
        AutoGetDb autoDb(opCtx, nss.db(), MODE_X);

        bool userInitiatedWritesAndNotPrimary = opCtx->writesAreReplicated() &&
            !repl::getGlobalReplicationCoordinator()->canAcceptWritesFor(opCtx, nss);

        if (userInitiatedWritesAndNotPrimary) {
            return {ErrorCodes::NotMaster,
                    str::stream() << "Not primary while dropping indexes in " << nss.ns()};
        }

        WriteUnitOfWork wunit(opCtx);
        Status status = wrappedRun(opCtx, nss, autoDb.getDb(), cmdObj, result);
        if (!status.isOK()) {
            return status;
        }

        wunit.commit();
    }
    MONGO_WRITE_CONFLICT_RETRY_LOOP_END(opCtx, "dropIndexes", nss.db());
    return Status::OK();
}

}  // namespace mongo
