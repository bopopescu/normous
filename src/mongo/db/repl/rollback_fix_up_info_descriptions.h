/**
 *    Copyright (C) 2017 MongoDB Inc.
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

#pragma once

#include "mongo/base/disallow_copying.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/repl/rollback_fix_up_info.h"
#include "mongo/util/uuid.h"

namespace mongo {
namespace repl {

/**
 * Represents a document in the "kRollbackDocsNamespace" namespace.
 */
class RollbackFixUpInfo::SingleDocumentOperationDescription {
    MONGO_DISALLOW_COPYING(SingleDocumentOperationDescription);

public:
    SingleDocumentOperationDescription(const UUID& collectionUuid,
                                       const BSONElement& docId,
                                       RollbackFixUpInfo::SingleDocumentOpType opType);

    /**
     * Returns a BSON representation of this object.
     */
    BSONObj toBSON() const;

private:
    UUID _collectionUuid;
    BSONObj _wrappedDocId;
    RollbackFixUpInfo::SingleDocumentOpType _opType;
};

/**
 * Represents a document in the "kCollectionUuidNamespace" namespace.
 * Contains information to roll back collection drops and renames.
 */
class RollbackFixUpInfo::CollectionUuidDescription {
    MONGO_DISALLOW_COPYING(CollectionUuidDescription);

public:
    CollectionUuidDescription(const UUID& collectionUuid, const NamespaceString& nss);

    /**
     * Returns a BSON representation of this object.
     */
    BSONObj toBSON() const;

private:
    UUID _collectionUuid;
    NamespaceString _nss;
};

/**
 * Represents a document in the "kCollectionOptionsNamespace" namespace.
 * Contains information to roll back non-TTL collMod operations.
 */
class RollbackFixUpInfo::CollectionOptionsDescription {
    MONGO_DISALLOW_COPYING(CollectionOptionsDescription);

public:
    CollectionOptionsDescription(const UUID& collectionUuid, const BSONObj& optionsObj);

    /**
     * Returns a BSON representation of this object.
     */
    BSONObj toBSON() const;

private:
    UUID _collectionUuid;
    BSONObj _optionsObj;
};

}  // namespace repl
}  // namespace mongo
