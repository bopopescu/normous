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

#pragma once

#include <map>

#include "mongo/db/logical_session_id.h"
#include "mongo/db/ops/single_write_result_gen.h"
#include "mongo/db/repl/optime.h"
#include "mongo/db/session_txn_record_gen.h"

namespace mongo {

class OperationContext;

/**
 * Represents the current state of this transaction.
 */
class SessionTxnState {
public:
    static const NamespaceString kConfigNS;
    using PartialResults = std::map<StmtId, SingleWriteResult>;

    SessionTxnState(LogicalSessionId sessionId, TxnNumber txnNumber);

    /**
     *  Load transaction state from storage if it hasn't.
     */
    void begin(OperationContext* opCtx);

    /**
     * Returns the partial results for this transaction.
     */
    const PartialResults& getPartialResults() const;

    /**
     * Stores the result of a single write operation within this transaction.
     */
    void storePartialResult(OperationContext* opCtx,
                            StmtId stmtId,
                            SingleWriteResult result,
                            repl::OpTime opTime);

    void addResultFromStorage(const SessionTxnRecord& txnRecord);

    const LogicalSessionId& getSessionId() const;
    const TxnNumber& getTxnNum() const;

    /**
     * Removes all previous transaction states in this logical session with transaction id smaller
     * than this one.
     */
    void cleanUpOlderTransactions(OperationContext* opCtx);

    /**
     * Returns a SessionTxnState stored in the operation context.
     */
    static SessionTxnState* get(OperationContext* opCtx);

    /**
     * Stores a TxnStateAccessToken object to an operation context.
     */
    static void set(OperationContext* opCtx, SessionTxnState* txnState);

private:
    const LogicalSessionId _sessionId;
    const TxnNumber _txnNumber;

    bool _isInitialized = false;
    PartialResults _partialResults;
    repl::OpTime _lastWriteOpTime;
};

}  // namespace mongo
