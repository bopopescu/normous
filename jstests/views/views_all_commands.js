/*
 * Declaratively-defined tests for views for all database commands. This file contains a map of test
 * definitions as well as code to run them.
 *
 * The example test
 *
 *      {
 *          command: {insert: "view", documents: [{x: 1}]},
 *          expectFailure: true
 *      }
 *
 * invokes runCommand with `command` and expects it to fail with an error code specific to views.
 * A test can be an array of subtests as well.
 *
 * Each test or subtest takes the following options:
 *
 *  command
 *      The command object to pass to db.runCommand(). Each command can assume that there exists a
 *      view named "view", built on top of a collection named "collection". A command can also be a
 *      function that takes a db handle as argument and handles pass/failure checking internally.
 *
 *  isAdminCommand
 *      If true, will execute 'command' against the admin db.
 *
 *  skip
 *      A string that, if present, causes the test runner to skip running this command altogether.
 *      The value should be the reason why the test is being skipped. (There is a predefined
 *      selection of commonly-used reasons below.)
 *
 *  expectFailure
 *      If true, assert that the command fails. Otherwise, all commands are expected to succeed.
 *
 *  expectedErrorCode
 *      When 'expectFailure' is true, specifies the error code expected. Defaults to
 *      'CommandNotSupportedOnView' when not specified. Set to 'null' when expecting an error
 *      without an error code field.
 *
 *  setup
 *      A function that will be run before the command is executed. It takes a handle to the 'test'
 *      database as its single argument.
 *
 *  teardown
 *      A function that will be run after the command is executed. It takes a handle to the 'test'
 *      database as its single argument.
 *
 *  skipSharded
 *      If true, do not run this command on a mongos.
 *
 *  skipStandalone
 *      If true, do not run this command on a standalone mongod.
 */

(function() {
    "use strict";

    // Pre-written reasons for skipping a test.
    const isAnInternalCommand = "internal command";
    const isUnrelated = "is unrelated";

    let viewsCommandTests = {
        _configsvrAddShard: {skip: isAnInternalCommand},
        _configsvrAddShardToZone: {skip: isAnInternalCommand},
        _configsvrBalancerStart: {skip: isAnInternalCommand},
        _configsvrBalancerStatus: {skip: isAnInternalCommand},
        _configsvrBalancerStop: {skip: isAnInternalCommand},
        _configsvrCommitChunkMigration: {skip: isAnInternalCommand},
        _configsvrMergeChunk: {skip: isAnInternalCommand},
        _configsvrMoveChunk: {skip: isAnInternalCommand},
        _configsvrRemoveShardFromZone: {skip: isAnInternalCommand},
        _configsvrSetFeatureCompatibilityVersion: {skip: isAnInternalCommand},
        _configsvrSplitChunk: {skip: isAnInternalCommand},
        _configsvrUpdateZoneKeyRange: {skip: isAnInternalCommand},
        _getUserCacheGeneration: {skip: isAnInternalCommand},
        _hashBSONElement: {skip: isAnInternalCommand},
        _isSelf: {skip: isAnInternalCommand},
        _mergeAuthzCollections: {skip: isAnInternalCommand},
        _migrateClone: {skip: isAnInternalCommand},
        _recvChunkAbort: {skip: isAnInternalCommand},
        _recvChunkCommit: {skip: isAnInternalCommand},
        _recvChunkStart: {skip: isAnInternalCommand},
        _recvChunkStatus: {skip: isAnInternalCommand},
        _transferMods: {skip: isAnInternalCommand},
        addShard: {skip: isUnrelated},
        addShardToZone: {skip: isUnrelated},
        aggregate: {command: {aggregate: "view", pipeline: [{$match: {}}]}},
        appendOplogNote: {skip: isUnrelated},
        applyOps: {
            command: {applyOps: [{op: "i", o: {_id: 1}, ns: "test.view"}]},
            expectFailure: true,
            skipSharded: true,
        },
        authSchemaUpgrade: {skip: isUnrelated},
        authenticate: {skip: isUnrelated},
        availableQueryOptions: {skip: isAnInternalCommand},
        balancerStart: {skip: isUnrelated},
        balancerStatus: {skip: isUnrelated},
        balancerStop: {skip: isUnrelated},
        buildInfo: {skip: isUnrelated},
        captrunc: {
            command: {captrunc: "view", n: 2, inc: false},
            expectFailure: true,
        },
        checkShardingIndex: {skip: isUnrelated},
        cleanupOrphaned: {
            skip: "Tested in views/views_sharded.js",
        },
        clone: {skip: "TODO(SERVER-24506)"},
        cloneCollection: {skip: "TODO(SERVER-24506)"},
        cloneCollectionAsCapped: {
            command: {cloneCollectionAsCapped: "view", toCollection: "testcapped", size: 10240},
            expectFailure: true,
        },
        collMod: {command: {collMod: "view", viewOn: "other", pipeline: []}},
        collStats: {command: {collStats: "view"}, skip: "TODO(SERVER-24568)"},
        compact: {command: {compact: "view", force: true}, expectFailure: true, skipSharded: true},
        configureFailPoint: {skip: isUnrelated},
        connPoolStats: {skip: isUnrelated},
        connPoolSync: {skip: isUnrelated},
        connectionStatus: {skip: isUnrelated},
        convertToCapped: {command: {convertToCapped: "view", size: 12345}, expectFailure: true},
        copydb: {skip: "TODO(SERVER-24506)"},
        copydbgetnonce: {skip: isUnrelated},
        copydbsaslstart: {skip: isUnrelated},
        count: {command: {count: "view"}},
        create: {skip: "tested in views/views_creation.js"},
        createIndexes: {
            command: {createIndexes: "view", indexes: [{key: {x: 1}, name: "x_1"}]},
            expectFailure: true,
        },
        createRole: {
            command: {createRole: "testrole", privileges: [], roles: []},
            setup: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllRolesFromDatabase: 1}));
            },
            teardown: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllRolesFromDatabase: 1}));
            }
        },
        createUser: {
            command: {createUser: "testuser", pwd: "testpass", roles: []},
            setup: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllUsersFromDatabase: 1}));
            },
            teardown: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllUsersFromDatabase: 1}));
            }
        },
        currentOp: {skip: isUnrelated},
        currentOpCtx: {skip: isUnrelated},
        dataSize: {
            command: {dataSize: "test.view"},
            expectFailure: true,
        },
        dbHash: {
            command: function(conn) {
                let getHash = function() {
                    let cmd = {dbHash: 1};
                    let res = conn.runCommand(cmd);
                    assert.commandWorked(res, tojson(cmd));
                    return res.collections["system.views"];
                };
                // The checksum below should change if we change the views, but not otherwise.
                let hash1 = getHash();
                assert.commandWorked(conn.runCommand({create: "view2", viewOn: "view"}),
                                     "could not create view 'view2' on 'view'");
                let hash2 = getHash();
                assert.neq(hash1, hash2, "expected hash to change after creating new view");
                assert.commandWorked(conn.runCommand({drop: "view2"}, "problem dropping view2"));
                let hash3 = getHash();
                assert.eq(hash1, hash3, "hash should be the same again after removing 'view2'");
            }
        },
        dbStats: {command: {dbStats: 1}, skip: "TODO(SERVER-24568)"},
        delete: {command: {delete: "view", deletes: [{q: {x: 1}, limit: 1}]}, expectFailure: true},
        diagLogging: {skip: isUnrelated},
        distinct: {command: {distinct: "view", key: "_id"}},
        driverOIDTest: {skip: isUnrelated},
        drop: {command: {drop: "view"}},
        dropAllRolesFromDatabase: {skip: isUnrelated},
        dropAllUsersFromDatabase: {skip: isUnrelated},
        dropDatabase: {command: {dropDatabase: 1}},
        dropIndexes: {command: {dropIndexes: "view"}, expectFailure: true},
        dropRole: {
            command: {dropRole: "testrole"},
            setup: function(conn) {
                assert.commandWorked(
                    conn.runCommand({createRole: "testrole", privileges: [], roles: []}));
            },
            teardown: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllRolesFromDatabase: 1}));
            }
        },
        dropUser: {skip: isUnrelated},
        emptycapped: {
            command: {emptycapped: "view"},
            expectFailure: true,
        },
        enableSharding: {skip: "Tested as part of shardCollection"},
        eval: {skip: isUnrelated},
        explain: {command: {explain: {count: "view"}}},
        features: {skip: isUnrelated},
        filemd5: {skip: isUnrelated},
        find: {skip: "tested in views/views_find.js & views/views_sharded.js"},
        findAndModify: {
            command: {findAndModify: "view", query: {a: 1}, update: {$set: {a: 2}}},
            expectFailure: true
        },
        flushRouterConfig: {skip: isUnrelated},
        forceerror: {skip: isUnrelated},
        fsync: {skip: isUnrelated},
        fsyncUnlock: {skip: isUnrelated},
        geoNear: {
            command:
                {geoNear: "view", near: {type: "Point", coordinates: [-50, 37]}, spherical: true},
            expectFailure: true
        },
        geoSearch: {
            command: {
                geoSearch: "view",
                search: {},
                near: [-50, 37],
            },
            expectFailure: true
        },
        getCmdLineOpts: {skip: isUnrelated},
        getDiagnosticData: {skip: isUnrelated},
        getLastError: {skip: isUnrelated},
        getLog: {skip: isUnrelated},
        getMore: {skip: "TODO(SERVER-24724)"},
        getParameter: {skip: isUnrelated},
        getPrevError: {skip: isUnrelated},
        getShardMap: {skip: isUnrelated},
        getShardVersion: {
            command: {getShardVersion: "test.view"},
            isAdminCommand: true,
            expectFailure: true,
            skipSharded: true,  // mongos is tested in views/views_sharded.js
        },
        getnonce: {skip: isUnrelated},
        godinsert: {skip: isAnInternalCommand},
        grantPrivilegesToRole: {
            command: {
                grantPrivilegesToRole: "testrole",
                privileges: [{resource: {db: "test", collection: "view"}, actions: ["find"]}]
            },
            setup: function(conn) {
                assert.commandWorked(
                    conn.runCommand({createRole: "testrole", privileges: [], roles: []}));
            },
            teardown: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllRolesFromDatabase: 1}));
            }
        },
        grantRolesToRole: {
            command: {grantRolesToRole: "testrole", roles: ["read"]},
            setup: function(conn) {
                assert.commandWorked(
                    conn.runCommand({createRole: "testrole", privileges: [], roles: []}));
            },
            teardown: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllRolesFromDatabase: 1}));
            }
        },
        grantRolesToUser: {
            command: {grantRolesToUser: "testuser", roles: ["read"]},
            setup: function(conn) {
                assert.commandWorked(
                    conn.runCommand({createUser: "testuser", pwd: "testpass", roles: []}));
            },
            teardown: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllUsersFromDatabase: 1}));
            },
            skip: "TODO(SERVER-24724)"
        },
        group: {
            command: {group: {ns: "test.view", key: "x", $reduce: function() {}, initial: {}}},
        },
        handshake: {skip: isUnrelated},
        hostInfo: {skip: isUnrelated},
        insert: {command: {insert: "view", documents: [{x: 1}]}, expectFailure: true},
        invalidateUserCache: {skip: isUnrelated},
        isdbgrid: {skip: isUnrelated},
        isMaster: {skip: isUnrelated},
        journalLatencyTest: {skip: isUnrelated},
        killCursors: {skip: "TODO(SERVER-24771)"},
        killOp: {skip: isUnrelated},
        listCollections: {skip: "tested in views/views_creation.js"},
        listCommands: {skip: isUnrelated},
        listDatabases: {skip: isUnrelated},
        listIndexes: {command: {listIndexes: "view"}, expectFailure: true},
        listShards: {skip: isUnrelated},
        lockInfo: {skip: isUnrelated},
        logApplicationMessage: {skip: isUnrelated},
        logRotate: {skip: isUnrelated},
        logout: {skip: isUnrelated},
        makeSnapshot: {skip: isAnInternalCommand},
        mapReduce: {
            command:
                {mapReduce: "view", map: function() {}, reduce: function(key, vals) {}, out: "out"},
            expectFailure: true
        },
        "mapreduce.shardedfinish": {skip: isAnInternalCommand},
        mergeChunks: {
            command: {mergeChunks: "test.view", bounds: [{x: 0}, {x: 10}]},
            setup: function(conn) {
                assert.commandWorked(conn.adminCommand({enableSharding: "test"}));
            },
            skipStandalone: true,
            isAdminCommand: true,
            expectFailure: true,
            expectedErrorCode: ErrorCodes.NamespaceNotSharded,
        },
        moveChunk: {
            command: {moveChunk: "test.view"},
            setup: function(conn) {
                assert.commandWorked(conn.adminCommand({enableSharding: "test"}));
            },
            skipStandalone: true,
            isAdminCommand: true,
            expectFailure: true,
            expectedErrorCode: ErrorCodes.NamespaceNotSharded,
        },
        movePrimary: {skip: "Tested in sharding/movePrimary1.js"},
        netstat: {skip: isAnInternalCommand},
        parallelCollectionScan: {command: {parallelCollectionScan: "view"}, expectFailure: true},
        ping: {command: {ping: 1}},
        planCacheClear: {command: {planCacheClear: "view"}, expectFailure: true},
        planCacheClearFilters: {command: {planCacheClearFilters: "view"}, expectFailure: true},
        planCacheListFilters: {command: {planCacheListFilters: "view"}, expectFailure: true},
        planCacheListPlans: {command: {planCacheListPlans: "view"}, expectFailure: true},
        planCacheListQueryShapes:
            {command: {planCacheListQueryShapes: "view"}, expectFailure: true},
        planCacheSetFilter: {command: {planCacheSetFilter: "view"}, expectFailure: true},
        profile: {skip: isUnrelated},
        reIndex: {command: {reIndex: "view"}, expectFailure: true},
        removeShard: {skip: isUnrelated},
        removeShardFromZone: {skip: isUnrelated},
        renameCollection: [
            {
              isAdminCommand: true,
              command: {renameCollection: "test.view", to: "test.otherview"},
              expectFailure: true,
            },
            {
              isAdminCommand: true,
              command: {renameCollection: "test.collection", to: "test.view"},
              expectFailure: true,
              expectedErrorCode: ErrorCodes.NamespaceExists,
            }
        ],
        repairCursor: {command: {repairCursor: "view"}, expectFailure: true},
        repairDatabase: {command: {repairDatabase: 1}},
        replSetElect: {skip: isUnrelated},
        replSetFreeze: {skip: isUnrelated},
        replSetFresh: {skip: isUnrelated},
        replSetGetConfig: {skip: isUnrelated},
        replSetGetRBID: {skip: isUnrelated},
        replSetGetStatus: {skip: isUnrelated},
        replSetHeartbeat: {skip: isUnrelated},
        replSetInitiate: {skip: isUnrelated},
        replSetMaintenance: {skip: isUnrelated},
        replSetReconfig: {skip: isUnrelated},
        replSetRequestVotes: {skip: isUnrelated},
        replSetStepDown: {skip: isUnrelated},
        replSetStepUp: {skip: isUnrelated},
        replSetSyncFrom: {skip: isUnrelated},
        replSetTest: {skip: isUnrelated},
        replSetUpdatePosition: {skip: isUnrelated},
        resetError: {skip: isUnrelated},
        resync: {skip: isUnrelated},
        revokePrivilegesFromRole: {
            command: {
                revokePrivilegesFromRole: "testrole",
                privileges: [{resource: {db: "test", collection: "view"}, actions: ["find"]}]
            },
            setup: function(conn) {
                assert.commandWorked(
                    conn.runCommand({createRole: "testrole", privileges: [], roles: []}));
            },
            teardown: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllRolesFromDatabase: 1}));
            }
        },
        revokeRolesFromRole: {skip: isUnrelated},
        revokeRolesFromUser: {skip: isUnrelated},
        rolesInfo: {skip: isUnrelated},
        saslContinue: {skip: isUnrelated},
        saslStart: {skip: isUnrelated},
        serverStatus: {command: {serverStatus: 1}, skip: isUnrelated},
        setCommittedSnapshot: {skip: isAnInternalCommand},
        setFeatureCompatibilityVersion: {skip: isUnrelated},
        setParameter: {skip: isUnrelated},
        setShardVersion: {skip: isUnrelated},
        shardCollection: {
            command: {shardCollection: "test.view", key: {_id: 1}},
            setup: function(conn) {
                assert.commandWorked(conn.adminCommand({enableSharding: "test"}));
            },
            skipStandalone: true,
            expectFailure: true,
            isAdminCommand: true,
        },
        shardConnPoolStats: {skip: isUnrelated},
        shardingState: {skip: isUnrelated},
        shutdown: {skip: isUnrelated},
        sleep: {skip: isUnrelated},
        split: {
            command: {split: "test.view", find: {_id: 1}},
            setup: function(conn) {
                assert.commandWorked(conn.adminCommand({enableSharding: "test"}));
            },
            skipStandalone: true,
            expectFailure: true,
            expectedErrorCode: ErrorCodes.NamespaceNotSharded,
            isAdminCommand: true,
        },
        splitChunk: {
            command: {
                splitChunk: "test.view",
                from: "shard0000",
                min: {x: MinKey},
                max: {x: 0},
                keyPattern: {x: 1},
                splitKeys: [{x: -2}, {x: -1}],
                shardVersion: [1, 2]
            },
            skipSharded: true,
            expectFailure: true,
            expectedErrorCode: null,
            isAdminCommand: true,
        },
        splitVector: {
            command: {
                splitVector: "test.view",
                keyPattern: {x: 1},
                maxChunkSize: 1,
            },
            expectFailure: true,
        },
        stageDebug: {skip: isAnInternalCommand},
        top: {command: {top: "view"}, isAdminCommand: true, skip: "TODO(SERVER-24568)"},
        touch: {
            command: {touch: "view", data: true},
            expectFailure: true,
        },
        unsetSharding: {skip: isAnInternalCommand},
        update: {command: {update: "view", updates: [{q: {x: 1}, u: {x: 2}}]}, expectFailure: true},
        updateRole: {
            command: {
                updateRole: "testrole",
                privileges: [{resource: {db: "test", collection: "view"}, actions: ["find"]}]
            },
            setup: function(conn) {
                assert.commandWorked(
                    conn.runCommand({createRole: "testrole", privileges: [], roles: []}));
            },
            teardown: function(conn) {
                assert.commandWorked(conn.runCommand({dropAllRolesFromDatabase: 1}));
            }
        },
        updateUser: {skip: isUnrelated},
        updateZoneKeyRange: {skip: isUnrelated},
        usersInfo: {skip: isUnrelated},
        validate: {command: {validate: "view"}, skip: "TODO(SERVER-24768)"},
        whatsmyuri: {skip: isUnrelated}
    };

    /**
     * Helper function for failing commands or writes that checks the result 'res' of either.
     * If 'code' is null we only check for failure, otherwise we confirm error code matches as
     * well. On assert 'msg' is printed.
     */
    let assertCommandOrWriteFailed = function(res, code, msg) {
        if (res.writeErrors !== undefined)
            assert.neq(0, res.writeErrors.length, msg);
        else if (res.code !== null)
            assert.commandFailedWithCode(res, code, msg);
        else
            assert.commandFailed(res, msg);
    };

    function runTests(db) {
        // Are we on a mongos?
        var isMaster = db.runCommand("ismaster");
        assert.commandWorked(isMaster);
        var isMongos = (isMaster.msg === "isdbgrid");

        // Obtain a list of all commands.
        let res = db.runCommand({listCommands: 1});
        assert.commandWorked(res);

        let commands = Object.keys(res.commands);
        for (let command of commands) {
            let test = viewsCommandTests[command];
            assert(test !== undefined,
                   "Coverage failure: must explicitly define a views test for " + command);

            if (!(test instanceof Array))
                test = [test];
            let subtest_nr = 0;
            for (let subtest of test) {
                // Tests can be explicitly skipped. Print the name of the skipped test, as well as
                // the reason why.
                if (subtest.skip !== undefined) {
                    print("Skipping " + command + ": " + subtest.skip);
                    continue;
                }

                let dbHandle = db.getSiblingDB("test");
                let commandHandle = dbHandle;

                // Skip tests depending on sharding configuration.
                if (subtest.skipSharded && isMongos) {
                    print("Skipping " + command + ": not applicable to mongoS");
                    continue;
                }

                if (subtest.skipStandalone && !isMongos) {
                    print("Skipping " + command + ": not applicable to mongoD");
                    continue;
                }

                // Perform test setup, and call any additional setup callbacks provided by the test.
                // All tests assume that there exists a view named 'view' that is backed by
                // 'collection'.
                assert.commandWorked(dbHandle.dropDatabase());
                assert.commandWorked(dbHandle.runCommand({create: "view", viewOn: "collection"}));
                assert.writeOK(dbHandle.collection.insert({x: 1}));
                if (subtest.setup !== undefined)
                    subtest.setup(dbHandle);

                // Execute the command. Print the command name for the first subtest, as otherwise
                // it may be hard to figure out what command caused a failure.
                if (!subtest_nr++)
                    print("Testing " + command);

                if (subtest.isAdminCommand)
                    commandHandle = db.getSiblingDB("admin");

                if (subtest.expectFailure) {
                    let expectedErrorCode = subtest.expectedErrorCode;
                    if (expectedErrorCode === undefined)
                        expectedErrorCode = ErrorCodes.CommandNotSupportedOnView;

                    assertCommandOrWriteFailed(commandHandle.runCommand(subtest.command),
                                               expectedErrorCode,
                                               tojson(subtest.command));
                } else if (subtest.command instanceof Function)
                    subtest.command(commandHandle);
                else
                    assert.commandWorked(commandHandle.runCommand(subtest.command),
                                         tojson(subtest.command));

                if (subtest.teardown !== undefined)
                    subtest.teardown(dbHandle);
            }
        }
    }

    // Run tests against mongoD.
    runTests(db.getSiblingDB("test"));

    // Run tests against mongoS.
    var st = new ShardingTest({shards: 2});
    runTests(st.s.getDB("test"));
    st.stop();
}());
