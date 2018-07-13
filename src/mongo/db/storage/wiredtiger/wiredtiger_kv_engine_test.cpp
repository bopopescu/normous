/**
 *    Copyright (C) 2014 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
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

#include "mongo/db/storage/kv/kv_engine_test_harness.h"

#include "mongo/base/init.h"
#include "mongo/db/repl/repl_settings.h"
#include "mongo/db/repl/replication_coordinator_mock.h"
#include "mongo/db/service_context.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_kv_engine.h"
#include "mongo/db/storage/wiredtiger/wiredtiger_record_store.h"
#include "mongo/stdx/memory.h"
#include "mongo/unittest/temp_dir.h"
#include "mongo/util/clock_source_mock.h"

namespace mongo {
namespace {

class WiredTigerKVHarnessHelper : public KVHarnessHelper {
public:
    WiredTigerKVHarnessHelper() : _dbpath("wt-kv-harness") {
        _engine.reset(new WiredTigerKVEngine(
            kWiredTigerEngineName, _dbpath.path(), _cs.get(), "", 1, false, false, false, false));
        repl::ReplicationCoordinator::set(
            getGlobalServiceContext(),
            std::unique_ptr<repl::ReplicationCoordinator>(new repl::ReplicationCoordinatorMock(
                getGlobalServiceContext(), repl::ReplSettings())));
    }

    virtual ~WiredTigerKVHarnessHelper() {
        _engine.reset(NULL);
    }

    virtual KVEngine* restartEngine() {
        _engine.reset(NULL);
        _engine.reset(new WiredTigerKVEngine(
            kWiredTigerEngineName, _dbpath.path(), _cs.get(), "", 1, false, false, false, false));
        return _engine.get();
    }

    virtual KVEngine* getEngine() {
        return _engine.get();
    }

private:
    const std::unique_ptr<ClockSource> _cs = stdx::make_unique<ClockSourceMock>();
    unittest::TempDir _dbpath;
    std::unique_ptr<WiredTigerKVEngine> _engine;
};

std::unique_ptr<KVHarnessHelper> makeHelper() {
    return stdx::make_unique<WiredTigerKVHarnessHelper>();
}

MONGO_INITIALIZER(RegisterKVHarnessFactory)(InitializerContext*) {
    KVHarnessHelper::registerFactory(makeHelper);
    return Status::OK();
}

}  // namespace
}  // namespace mongo
