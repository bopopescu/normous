/**
 * Copyright (C) 2018 MongoDB Inc.
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
 *
 * As a special exception, the copyright holders give permission to link the
 * code of portions of this program with the OpenSSL library under certain
 * conditions as described in each individual source file and distribute
 * linked combinations including the program with the OpenSSL library. You
 * must comply with the GNU Affero General Public License in all respects
 * for all of the code used other than as permitted herein. If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version. If you
 * delete this exception statement from all source files in the program,
 * then also delete it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kStorage

#include "mongo/platform/basic.h"

#include "mongo/db/periodic_runner_job_abort_expired_transactions.h"

#include "mongo/db/client.h"
#include "mongo/db/kill_sessions_local.h"
#include "mongo/db/service_context.h"
#include "mongo/db/session.h"
#include "mongo/util/log.h"
#include "mongo/util/periodic_runner.h"

namespace mongo {

void startPeriodicThreadToAbortExpiredTransactions(ServiceContext* serviceContext) {
    // Enforce calling this function once, and only once.
    static bool firstCall = true;
    invariant(firstCall);
    firstCall = false;

    auto periodicRunner = serviceContext->getPeriodicRunner();
    invariant(periodicRunner);

    // We want this job period to be dynamic, to run every (transactionLifetimeLimitSeconds/2)
    // seconds, where transactionLifetimeLimitSeconds is an adjustable server parameter, or within
    // the 1 second to 1 minute range.
    //
    // PeriodicRunner does not currently support altering the period of a job. So we are giving this
    // job a 1 second period on PeriodicRunner and incrementing a static variable 'seconds' on each
    // run until we reach transactionLifetimeLimitSeconds/2, at which point we run the code and
    // reset 'seconds'. Etc.
    PeriodicRunner::PeriodicJob job(
        [](Client* client) {
            try {
                static int seconds = 0;
                int lifetime = transactionLifetimeLimitSeconds.load();

                invariant(lifetime >= 1);
                int period = lifetime / 2;

                // Ensure: 1 <= period <= 60 seconds
                period = (period < 1) ? 1 : period;
                period = (period > 60) ? 60 : period;

                if (++seconds < period) {
                    return;
                }

                seconds = 0;

                // The opCtx destructor handles unsetting itself from the Client.
                // (The PeriodicRunnerASIO's Client must be reset before returning.)
                auto opCtx = client->makeOperationContext();

                killAllExpiredTransactions(opCtx.get());
            } catch (const DBException& ex) {
                if (!ErrorCodes::isShutdownError(ex.toStatus().code())) {
                    warning() << "Periodic task to abort expired transactions failed! Caused by: "
                              << ex.toStatus();
                }
            }
        },
        Seconds(1));

    periodicRunner->scheduleJob(std::move(job));
}

}  // namespace mongo
