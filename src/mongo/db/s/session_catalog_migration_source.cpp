/**
 *    Copyright (C) 2017 MongoDB, Inc.
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

#include "mongo/platform/basic.h"

#include "mongo/db/s/session_catalog_migration_source.h"

#include "mongo/db/concurrency/write_conflict_exception.h"
#include "mongo/db/db_raii.h"
#include "mongo/db/dbdirectclient.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/repl/repl_client_info.h"
#include "mongo/db/repl/replication_process.h"
#include "mongo/db/session.h"
#include "mongo/db/session_txn_record_gen.h"
#include "mongo/db/transaction_history_iterator.h"
#include "mongo/db/write_concern.h"
#include "mongo/platform/random.h"
#include "mongo/stdx/memory.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

namespace {

PseudoRandom hashGenerator(std::unique_ptr<SecureRandom>(SecureRandom::create())->nextInt64());

boost::optional<repl::OplogEntry> fetchPrePostImageOplog(OperationContext* opCtx,
                                                         const repl::OplogEntry& oplog) {
    auto opTimeToFetch = oplog.getPreImageOpTime();

    if (!opTimeToFetch) {
        opTimeToFetch = oplog.getPostImageOpTime();
    }

    if (!opTimeToFetch) {
        return boost::none;
    }

    auto opTime = opTimeToFetch.value();
    DBDirectClient client(opCtx);
    auto oplogBSON = client.findOne(NamespaceString::kRsOplogNamespace.ns(), opTime.asQuery());

    return uassertStatusOK(repl::OplogEntry::parse(oplogBSON));
}

}  // unnamed namespace

SessionCatalogMigrationSource::SessionCatalogMigrationSource(NamespaceString ns)
    : _ns(std::move(ns)) {}

bool SessionCatalogMigrationSource::hasMoreOplog() {
    return _hasMoreOplogFromSessionCatalog() || _hasNewWrites();
}

SessionCatalogMigrationSource::OplogResult SessionCatalogMigrationSource::getLastFetchedOplog() {
    {
        stdx::lock_guard<stdx::mutex> _lk(_sessionCloneMutex);
        if (_lastFetchedOplog) {
            return OplogResult(_lastFetchedOplog, false);
        }
    }

    {
        stdx::lock_guard<stdx::mutex> _lk(_newOplogMutex);
        return OplogResult(_lastFetchedNewWriteOplog, true);
    }
}

bool SessionCatalogMigrationSource::fetchNextOplog(OperationContext* opCtx) {
    if (_fetchNextOplogFromSessionCatalog(opCtx)) {
        return true;
    }

    return _fetchNextNewWriteOplog(opCtx);
}

bool SessionCatalogMigrationSource::_handleWriteHistory(WithLock, OperationContext* opCtx) {
    if (_currentOplogIterator) {
        if (_currentOplogIterator->hasNext()) {
            auto nextOplog = _currentOplogIterator->getNext(opCtx);
            auto nextStmtId = nextOplog.getStatementId();

            // Note: This is an optimization based on the assumption that it is not possible to be
            // touching different namespaces in the same transaction.
            if (!nextStmtId || (nextStmtId && *nextStmtId != kIncompleteHistoryStmtId &&
                                nextOplog.getNamespace() != _ns)) {
                _currentOplogIterator.reset();
                return false;
            }

            auto doc = fetchPrePostImageOplog(opCtx, nextOplog);
            if (doc) {
                _lastFetchedOplogBuffer.push_back(nextOplog);
                _lastFetchedOplog = *doc;
            } else {
                _lastFetchedOplog = nextOplog;
            }

            return true;
        } else {
            _currentOplogIterator.reset();
        }
    }

    return false;
}

bool SessionCatalogMigrationSource::_hasMoreOplogFromSessionCatalog() {
    stdx::lock_guard<stdx::mutex> _lk(_sessionCloneMutex);
    return _lastFetchedOplog || !_lastFetchedOplogBuffer.empty() ||
        !_sessionOplogIterators.empty() || _currentOplogIterator;
}

// Important: The no-op oplog entry for findAndModify should always be returned first before the
// actual operation.
repl::OplogEntry SessionCatalogMigrationSource::_getLastFetchedOplogFromSessionCatalog() {
    stdx::lock_guard<stdx::mutex> lk(_sessionCloneMutex);
    return _lastFetchedOplogBuffer.back();
}

bool SessionCatalogMigrationSource::_fetchNextOplogFromSessionCatalog(OperationContext* opCtx) {
    stdx::unique_lock<stdx::mutex> lk(_sessionCloneMutex);

    invariant(_alreadyInitialized);
    if (!_lastFetchedOplogBuffer.empty()) {
        _lastFetchedOplog = _lastFetchedOplogBuffer.back();
        _lastFetchedOplogBuffer.pop_back();
        return true;
    }

    _lastFetchedOplog.reset();

    if (_handleWriteHistory(lk, opCtx)) {
        return true;
    }

    while (!_sessionOplogIterators.empty()) {
        _currentOplogIterator = std::move(_sessionOplogIterators.back());
        _sessionOplogIterators.pop_back();

        if (_handleWriteHistory(lk, opCtx)) {
            return true;
        }
    }

    return false;
}

bool SessionCatalogMigrationSource::_hasNewWrites() {
    stdx::lock_guard<stdx::mutex> lk(_newOplogMutex);
    return _lastFetchedNewWriteOplog || !_newWriteOpTimeList.empty();
}

repl::OplogEntry SessionCatalogMigrationSource::_getLastFetchedNewWriteOplog() {
    stdx::lock_guard<stdx::mutex> lk(_newOplogMutex);
    invariant(_lastFetchedNewWriteOplog);
    return *_lastFetchedNewWriteOplog;
}

bool SessionCatalogMigrationSource::_fetchNextNewWriteOplog(OperationContext* opCtx) {
    repl::OpTime nextOpTimeToFetch;

    {
        stdx::lock_guard<stdx::mutex> lk(_newOplogMutex);

        invariant(_alreadyInitialized);
        if (_newWriteOpTimeList.empty()) {
            _lastFetchedNewWriteOplog.reset();
            return false;
        }

        nextOpTimeToFetch = _newWriteOpTimeList.front();
    }

    DBDirectClient client(opCtx);
    auto newWriteOplog =
        client.findOne(NamespaceString::kRsOplogNamespace.ns(), nextOpTimeToFetch.asQuery());

    uassert(40620,
            str::stream() << "Unable to fetch oplog entry with opTime: "
                          << nextOpTimeToFetch.toBSON(),
            !newWriteOplog.isEmpty());

    {
        stdx::lock_guard<stdx::mutex> lk(_newOplogMutex);
        _lastFetchedNewWriteOplog = uassertStatusOK(repl::OplogEntry::parse(newWriteOplog));
        _newWriteOpTimeList.pop_front();
    }

    return true;
}

void SessionCatalogMigrationSource::notifyNewWriteOpTime(repl::OpTime opTime) {
    stdx::lock_guard<stdx::mutex> lk(_newOplogMutex);
    _newWriteOpTimeList.push_back(opTime);
}

void SessionCatalogMigrationSource::init(OperationContext* opCtx) {
    invariant(!_alreadyInitialized);

    _rollbackIdAtInit = uassertStatusOK(repl::ReplicationProcess::get(opCtx)->getRollbackID(opCtx));

    DBDirectClient client(opCtx);
    Query query;
    // Sort is not needed for correctness. This is just for making it easier to write deterministic
    // tests.
    query.sort(BSON("_id" << 1));
    auto cursor = client.query(NamespaceString::kSessionTransactionsTableNamespace.ns(), query);

    std::vector<std::unique_ptr<SessionOplogIterator>> sessionOplogIterators;
    while (cursor->more()) {
        auto nextSession = SessionTxnRecord::parse(
            IDLParserErrorContext("Session migration cloning"), cursor->next());
        if (!nextSession.getLastWriteOpTime().isNull()) {
            sessionOplogIterators.push_back(
                stdx::make_unique<SessionOplogIterator>(std::move(nextSession), _rollbackIdAtInit));
        }
    }

    {
        auto message = BSON("sessionMigrateCloneStart" << _ns.ns());
        AutoGetCollection autoColl(opCtx, NamespaceString::kRsOplogNamespace, MODE_IX);
        writeConflictRetry(
            opCtx,
            "session migration initialization majority commit barrier",
            NamespaceString::kRsOplogNamespace.ns(),
            [&] {
                WriteUnitOfWork wuow(opCtx);
                opCtx->getClient()->getServiceContext()->getOpObserver()->onInternalOpMessage(
                    opCtx, _ns, {}, {}, message);
                wuow.commit();
            });
    }

    auto opTimeToWait = repl::ReplClientInfo::forClient(opCtx->getClient()).getLastOp();
    WriteConcernResult result;
    WriteConcernOptions majority(
        WriteConcernOptions::kMajority, WriteConcernOptions::SyncMode::UNSET, 0);
    uassertStatusOK(waitForWriteConcern(opCtx, opTimeToWait, majority, &result));

    stdx::lock_guard<stdx::mutex> lk(_sessionCloneMutex);
    _alreadyInitialized = true;
    _sessionOplogIterators.swap(sessionOplogIterators);
}

SessionCatalogMigrationSource::SessionOplogIterator::SessionOplogIterator(
    SessionTxnRecord txnRecord, int expectedRollbackId)
    : _record(std::move(txnRecord)), _initialRollbackId(expectedRollbackId) {
    _writeHistoryIterator =
        stdx::make_unique<TransactionHistoryIterator>(_record.getLastWriteOpTime());
}

bool SessionCatalogMigrationSource::SessionOplogIterator::hasNext() const {
    return _writeHistoryIterator && _writeHistoryIterator->hasNext();
}

repl::OplogEntry SessionCatalogMigrationSource::SessionOplogIterator::getNext(
    OperationContext* opCtx) {
    try {
        // Note: during SessionCatalogMigrationSource::init, we inserted a document and wait for it
        // to committed to the majority. In addition, the TransactionHistoryIterator uses OpTime
        // to query for the oplog. This means that if we can successfully fetch the oplog, we are
        // guaranteed that they are majority committed. If we can't fetch the oplog, it can either
        // mean that the oplog has been rolled over or was rolled back.
        return _writeHistoryIterator->next(opCtx);
    } catch (const AssertionException& excep) {
        if (excep.code() == ErrorCodes::IncompleteTransactionHistory) {
            // Note: no need to check if in replicaSet mode because having an iterator implies
            // oplog exists.
            auto rollbackId =
                uassertStatusOK(repl::ReplicationProcess::get(opCtx)->getRollbackID(opCtx));

            uassert(40656,
                    str::stream() << "rollback detected, rollbackId was " << _initialRollbackId
                                  << " but is now "
                                  << rollbackId,
                    rollbackId == _initialRollbackId);

            // If the rollbackId hasn't changed, this means that the oplog has been truncated.
            // So, we return the special "write  history lost" sentinel.
            repl::OplogEntry oplog({},
                                   hashGenerator.nextInt64(),
                                   repl::OpTypeEnum::kNoop,
                                   {},
                                   repl::OplogEntry::kOplogVersion,
                                   {},
                                   Session::kDeadEndSentinel);
            OperationSessionInfo sessionInfo;
            sessionInfo.setSessionId(_record.getSessionId());
            sessionInfo.setTxnNumber(_record.getTxnNum());
            oplog.setOperationSessionInfo(sessionInfo);
            oplog.setStatementId(kIncompleteHistoryStmtId);

            _writeHistoryIterator.reset();

            return oplog;
        }

        throw;
    }
}

}  // namespace mongo
