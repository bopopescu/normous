/*

Declaratively defined tests for the authorization properties
of all database commands.

This file contains an array of test definitions, as well as
some shared test logic.

See jstests/auth/commands_builtinRoles.js and
jstests/auth/commands_userDefinedRoles.js for two separate implementations
of the test logic, respectively to test authorization with builtin roles
and authorization with user-defined roles.

Example test definition:

This tests that when run on the "roles_commands_1" database,
the given command will return an authorization error unless
the user has the role with key "readWrite" or the role with
key "readWriteAnyDatabase". The command will be run as a
user with each of the roles in the "roles" array below.

Similarly, this tests that when run on the "roles_commands_2"
database, only a user with role "readWriteAnyDatabase" should
be authorized.

    {
        testname: "aggregate_write",
        command: {aggregate: "foo", pipeline: [ {$out: "foo_out"} ] },
        testcases: [
            { runOnDb: "roles_commands_1", rolesAllowed: {readWrite: 1, readWriteAnyDatabase: 1} },
            { runOnDb: "roles_commands_2", rolesAllowed: {readWriteAnyDatabase: 1} }
        ]
    },

Additional options:

1) onSuccess

A test can provide an onSuccess callback, which is called when the command
is authorized and succeeds. The callback is passed a single parameter, which
is the document returned by the command.

2) expectFail

You can add "expectFail: true" to an individual element of the testcases
array. This means that if the command is authorized, then you still expect
it to fail with a non-auth related error. As always, for roles other than
those in rolesAllowed, an auth error is expected.

3) skipSharded

Add "skipSharded: true" if you want to run the test only on a standalone.

4) skipStandalone

Add "skipStandalone: true" if you want to run the test only in sharded
configuration.

5) setup

The setup function, if present, is called before testing whether a
particular role authorizes a command for a particular database.

6) teardown

The teardown function, if present, is called immediately after
testint whether a particular role authorizes a command for a
particular database.

*/

// constants
var firstDbName = "roles_commands_1";
var secondDbName = "roles_commands_2";
var adminDbName = "admin";
var authErrCode = 13;
var shard0name = "shard0000";

// useful shorthand when defining the tests below
var roles_write = {
    readWrite: 1,
    readWriteAnyDatabase: 1,
    dbOwner: 1,
    root: 1,
    __system: 1
};
var roles_readWrite = {
    read: 1,
    readAnyDatabase: 1,
    readWrite: 1,
    readWriteAnyDatabase: 1,
    dbOwner: 1,
    root: 1,
    __system: 1
};
var roles_readWriteAny = {
    readAnyDatabase: 1,
    readWriteAnyDatabase: 1,
    root: 1,
    __system: 1
};
var roles_dbAdmin = {
    dbAdmin: 1,
    dbAdminAnyDatabase: 1,
    dbOwner: 1,
    root: 1,
    __system: 1
};
var roles_dbAdminAny = {
    dbAdminAnyDatabase: 1,
    root: 1,
    __system: 1
};
var roles_writeDbAdmin = {
    readWrite: 1,
    readWriteAnyDatabase: 1,
    dbAdmin: 1,
    dbAdminAnyDatabase: 1,
    dbOwner: 1,
    root: 1,
    __system: 1 
};
var roles_writeDbAdminAny = {
    readWriteAnyDatabase: 1,
    dbAdminAnyDatabase: 1,
    root: 1,
    __system: 1
};
var roles_readWriteDbAdmin = {
    read: 1,
    readAnyDatabase: 1,
    readWrite: 1,
    readWriteAnyDatabase: 1,
    dbAdmin: 1,
    dbAdminAnyDatabase: 1,
    dbOwner: 1,
    root: 1,
    __system: 1
};
var roles_readWriteDbAdminAny = {
    readAnyDatabase: 1,
    readWriteAnyDatabase: 1,
    dbAdminAnyDatabase: 1,
    root: 1,
    __system: 1
};
var roles_monitoring = {
    clusterMonitor: 1,
    clusterAdmin: 1,
    root: 1,
    __system: 1
};
var roles_hostManager = {
    hostManager: 1,
    clusterAdmin: 1,
    root: 1,
    __system: 1
};
var roles_clusterManager = {
    clusterManager: 1,
    clusterAdmin: 1,
    root: 1,
    __system: 1
};
var roles_all = {
    read: 1,
    readAnyDatabase: 1,
    readWrite: 1,
    readWriteAnyDatabase: 1,
    userAdmin: 1,
    userAdminAnyDatabase: 1,
    dbAdmin: 1,
    dbAdminAnyDatabase: 1,
    dbOwner: 1,
    clusterMonitor: 1,
    hostManager: 1,
    clusterManager: 1,
    clusterAdmin: 1,
    root: 1,
    __system: 1
};

var authCommandsLib = {



    /************* TEST CASES ****************/

    tests: [
        {
            testname: "addShard",
            command: {addShard: "x"},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["addShard"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "applyOps",
            command: {applyOps: "x"},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {anyResource: true}, actions: ["anyAction"] }
                    ],
                    expectFail: true
                },
                {
                    runOnDb: firstDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {anyResource: true}, actions: ["anyAction"] }
                    ],
                    expectFail: true
                }
            ]
        },
        {
            testname: "aggregate_readonly",
            command: {aggregate: "foo", pipeline: []},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "foo"}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "foo"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "aggregate_explain",
            command: {aggregate: "foo", explain: true, pipeline: [ {$match: {bar: 1}} ] },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "foo"}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "foo"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "aggregate_write",
            command: {aggregate: "foo", pipeline: [ {$out: "foo_out"} ] },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_write,
                    /* SERVER-10963
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "foo"}, actions: ["find"] },
                        { resource: {db: firstDbName, collection: "foo_out"}, actions: ["insert"] },
                        { resource: {db: firstDbName, collection: "foo_out"}, actions: ["remove"] }
                    ]
                    */
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {readWriteAnyDatabase: 1, root: 1, __system: 1},
                    /* SERVER-10963
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "foo"}, actions: ["find"] },
                        { resource: {db: secondDbName, collection: "foo_out"}, actions: ["insert"] },
                        { resource: {db: secondDbName, collection: "foo_out"}, actions: ["remove"] }
                    ]
                    */
                }
            ]
        },
        {
            testname: "buildInfo",
            command: {buildInfo: 1},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_all,
                    requiredPrivileges: [ ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_all,
                    requiredPrivileges: [ ]
                }
            ]
        },
        {
            testname: "checkShardingIndex_firstDb",
            command: {checkShardingIndex: firstDbName + ".x", keyPattern: {_id: 1} },
            skipSharded: true,
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "checkShardingIndex_secondDb",
            command: {checkShardingIndex: secondDbName + ".x", keyPattern: {_id: 1} },
            skipSharded: true,
            testcases: [
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "cloneCollection_1",
            command: {cloneCollection: firstDbName + ".x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_write,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["insert", "createIndex"] }
                    ],
                    expectFail: true
                }
            ]
        },
        {
            testname: "cloneCollection_2",
            command: {cloneCollection: secondDbName + ".x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {readWriteAnyDatabase: 1, root: 1, __system: 1},
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["insert", "createIndex"] }
                    ],
                    expectFail: true
                }
            ]
        },
        {
            testname: "cloneCollectionAsCapped",
            command: {cloneCollectionAsCapped: "x", toCollection: "y", size: 1000},
            skipSharded: true,
            setup: function (db) { db.x.save( {} ); },
            teardown: function (db) {
                db.x.drop();
                db.y.drop();
            },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_write,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "y"}, actions: ["insert", "createIndex", "convertToCapped"] },
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {readWriteAnyDatabase: 1, root: 1, __system: 1},
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "y"}, actions: ["insert", "createIndex", "convertToCapped"] },
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "closeAllDatabases",
            command: {closeAllDatabases: "x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["closeAllDatabases"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "collMod",
            command: {collMod: "foo", usePowerOf2Sizes: true},
            setup: function (db) { db.foo.save( {} ); },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_dbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "foo"}, actions: ["collMod"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_dbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "foo"}, actions: ["collMod"] }
                    ]
                }
            ]
        },
        {
            testname: "collStats",
            command: {collStats: "bar", scale: 1},
            setup: function (db) { db.bar.save( {} ); },
            teardown: function (db) { db.dropDatabase(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: {
                        read: 1,
                        readAnyDatabase: 1,
                        readWrite: 1,
                        readWriteAnyDatabase: 1,
                        dbAdmin: 1,
                        dbAdminAnyDatabase: 1,
                        dbOwner: 1,
                        clusterMonitor: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "bar"}, actions: ["collStats"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {
                        readAnyDatabase: 1,
                        readWriteAnyDatabase: 1,
                        dbAdminAnyDatabase: 1,
                        clusterMonitor: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "bar"}, actions: ["collStats"] }
                    ]
                }
            ]
        },
        {
            testname: "compact",
            command: {compact: "foo"},
            skipSharded: true,
            setup: function (db) { db.foo.save( {} ); },
            teardown: function (db) { db.dropDatabase(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_dbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "foo"}, actions: ["compact"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_dbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "foo"}, actions: ["compact"] }
                    ]
                }
            ]
        },
        {
            testname: "connectionStatus",
            command: {connectionStatus: 1},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_all,
                    requiredPrivileges: [ ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_all,
                    requiredPrivileges: [ ]
                }
            ]
        },
        {
            testname: "connPoolStats",
            command: {connPoolStats: 1},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["connPoolStats"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["connPoolStats"] }
                    ]
                }
            ]
        },
        {
            testname: "connPoolSync",
            command: {connPoolSync: 1},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["connPoolSync"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["connPoolSync"] }
                    ]
                }
            ]
        },
        {
            testname: "convertToCapped",
            command: {convertToCapped: "toCapped", size: 1000},
            setup: function (db) { db.toCapped.save( {} ); },
            teardown: function (db) { db.toCapped.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_writeDbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "toCapped"}, actions:["convertToCapped"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_writeDbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "toCapped"}, actions:["convertToCapped"] }
                    ]
                }
            ]
        },
        {
            testname: "count",
            command: {count: "x"},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "create",
            command: {create: "x"},
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_writeDbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["createCollection"] }
                    ]
                },
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_writeDbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["insert"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_writeDbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["createCollection"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_writeDbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["insert"] }
                    ]
                }
            ]
        },
        {
            testname: "create_capped",
            command: {create: "x", capped: true, size: 1000},
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_writeDbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["createCollection", "convertToCapped"] }
                    ]
                },
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_writeDbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["insert", "convertToCapped"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_writeDbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["createCollection", "convertToCapped"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_writeDbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["insert", "convertToCapped"] }
                    ]
                }
            ]
        },
        {
            testname: "cursorInfo",
            command: {cursorInfo: 1},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["cursorInfo"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["cursorInfo"] }
                    ]
                }
            ]
        },
        {
            testname: "dataSize_1",
            command: {dataSize: firstDbName + ".x"},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "dataSize_2",
            command: {dataSize: secondDbName + ".x"},
            testcases: [
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "dbHash",
            command: {dbHash: 1},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: ""}, actions: ["dbHash"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: ""}, actions: ["dbHash"] }
                    ]
                }
            ]
        },
        {
            testname: "dbStats",
            command: {dbStats: 1, scale: 1024},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: {
                        read: 1,
                        readAnyDatabase: 1,
                        readWrite: 1,
                        readWriteAnyDatabase: 1,
                        dbAdmin: 1,
                        dbAdminAnyDatabase: 1,
                        dbOwner: 1,
                        clusterMonitor: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: ""}, actions: ["dbStats"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {
                        readAnyDatabase: 1,
                        readWriteAnyDatabase: 1,
                        dbAdminAnyDatabase: 1,
                        clusterMonitor: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: ""}, actions: ["dbStats"] }
                    ]
                }
            ]
        },
        {
            testname: "diagLogging",
            command: {diagLogging: 1},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["diagLogging"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "distinct",
            command: {distinct: "coll", key: "a", query: {}},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "coll"}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "coll"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "drop",
            command: {drop: "x"},
            setup: function (db) { db.x.save({}); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_writeDbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["dropCollection"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_writeDbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["dropCollection"] }
                    ]
                }
            ]
        },
        {
            testname: "dropDatabase",
            command: {dropDatabase: 1},
            setup: function (db) { db.x.save({}); },
            teardown: function (db) { db.x.save({}); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: {
                        dbAdmin: 1,
                        dbAdminAnyDatabase: 1,
                        dbOwner: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: ""}, actions: ["dropDatabase"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {
                        dbAdminAnyDatabase: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: ""}, actions: ["dropDatabase"] }
                    ]
                }
            ]
        },
        {
            testname: "dropIndexes",
            command: {dropIndexes: "x", index: "*"},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_writeDbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["dropIndex"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_writeDbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["dropIndex"] }
                    ]
                }
            ]
        },
        {
            testname: "enableSharding",
            command: {enableSharding: "x"},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "x", collection: ""}, actions: ["enableSharding"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "eval",
            command: {$eval: function () { print("noop"); } },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {anyResource: true}, actions: ["anyAction"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {anyResource: true}, actions: ["anyAction"] }
                    ]
                }
            ]
        },
        {
            testname: "features",
            command: {features: 1},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_all,
                    privilegesRequired: [ ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_all,
                    privilegesRequired: [ ]
                }
            ]
        },
        {
            testname: "filemd5",
            command: {filemd5: 1, root: "fs"},
            setup: function (db) {
                db.fs.chunks.drop();
                db.fs.chunks.insert({files_id: 1, n: 0, data: new BinData(0, "test")});
                db.fs.chunks.ensureIndex({files_id: 1, n: 1});
            },
            teardown: function (db) {
                db.fs.chunks.drop();
            },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: ""}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: ""}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "findAndModify",
            command: {findAndModify: "x", query: {_id: "abc"}, update: {$inc: {n: 1}}},
            setup: function (db) {
                db.x.drop();
                db.x.save( {_id: "abc", n: 0} );
            },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_write,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find", "update"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {readWriteAnyDatabase: 1, root: 1, __system: 1},
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find", "update"] }
                    ]
                }
            ]
        },
        {
            testname: "flushRouterConfig",
            command: {flushRouterConfig: 1},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["flushRouterConfig"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "fsync",
            command: {fsync: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["fsync"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "geoNear",
            command: {geoNear: "x", near: [50, 50], num: 1},
            setup: function (db) {
                db.x.drop();
                db.x.save({loc: [50, 50]});
                db.x.ensureIndex({loc: "2d"});
            },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "geoSearch",
            command: {geoSearch: "x", near: [50, 50], maxDistance: 6, limit: 1, search: {} },
            skipSharded: true,
            setup: function (db) {
                db.x.drop();
                db.x.save({loc: {long: 50, lat: 50}});
                db.x.ensureIndex({loc: "geoHaystack", type: 1}, {bucketSize: 1});
            },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "getCmdLineOpts",
            command: {getCmdLineOpts: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["getCmdLineOpts"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "getLastError",
            command: {getLastError: 1},
            testcases: [
                { runOnDb: firstDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: secondDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] }
            ]
        },
        {
            testname: "getLog",
            command: {getLog: "*"},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["getLog"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "getnonce",
            command: {getnonce: 1},
            testcases: [
                { runOnDb: firstDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: secondDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] }
            ]
        },
        {
            testname: "getParameter",
            command: {getParameter: 1, quiet: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["getParameter"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "getPrevError",
            command: {getPrevError: 1},
            skipSharded: true,
            testcases: [
                { runOnDb: firstDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: secondDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] }
            ]
        },
        {
            testname: "getoptime",
            command: {getoptime: 1},
            skipSharded: true,
            testcases: [
                { runOnDb: firstDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: secondDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] }
            ]
        },
        {
            testname: "getShardMap",
            command: {getShardMap: "x"},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["getShardMap"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "getShardVersion",
            command: {getShardVersion: "test.foo"},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {db: "test", collection: 'foo'}, actions: ["getShardVersion"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "group",
            command: {
                group: {
                    ns: "x",
                    key: {groupby: 1},
                    initial: {total: 0},
                    $reduce: function (curr, result) {
                        result.total += curr.n;
                    }
                }
            },
            setup: function (db) {
                db.x.insert({groupby: 1, n: 5});
                db.x.insert({groupby: 1, n: 6});
            },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "hostInfo",
            command: {hostInfo: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["hostInfo"] }
                    ]
                },
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["hostInfo"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["hostInfo"] }
                    ]
                }
            ]
        },
        {
            testname: "indexStats",
            command: {indexStats: "x", index: "a_1"},
            skipSharded: true, 
            setup: function (db) {
                db.x.save({a: 10});
                db.x.ensureIndex({a: 1});
            },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_dbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["indexStats"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_dbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["indexStats"] }
                    ]
                }
            ]
        },
        {
            testname: "isMaster",
            command: {isMaster: 1},
            testcases: [
                { runOnDb: adminDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: firstDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: secondDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] }
            ]
        },
        {
            testname: "listCommands",
            command: {listCommands: 1},
            testcases: [
                { runOnDb: adminDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: firstDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: secondDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] }
            ]
        },
        {
            testname: "listDatabases",
            command: {listDatabases: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {
                        readAnyDatabase: 1,
                        readWriteAnyDatabase: 1,
                        dbAdminAnyDatabase: 1,
                        userAdminAnyDatabase: 1,
                        clusterMonitor: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["listDatabases"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "listShards",
            command: {listShards: 1},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["listShards"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "logRotate",
            command: {logRotate: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["logRotate"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "mapReduce_readonly",
            command: {
                mapreduce: "x",
                map: function () { emit(this.groupby, this.n); },
                reduce: function (id,emits) { return Array.sum(emits); },
                out: {inline: 1}
            },
            setup: function (db) {
                db.x.insert({groupby: 1, n: 5});
                db.x.insert({groupby: 1, n: 6});
            },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] }
                    ]
                }
            ]
        },
        {
            testname: "mapReduce_write",
            command: {
                mapreduce: "x",
                map: function () { emit(this.groupby, this.n); },
                reduce: function (id,emits) { return Array.sum(emits); },
                out: "mr_out"
            },
            setup: function (db) {
                db.x.insert({groupby: 1, n: 5});
                db.x.insert({groupby: 1, n: 6});
            },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_write,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] },
                        { resource: {db: firstDbName, collection: "mr_out"}, actions: ["insert", "remove"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {readWriteAnyDatabase: 1, root: 1, __system: 1},
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] },
                        { resource: {db: secondDbName, collection: "mr_out"}, actions: ["insert", "remove"] }
                    ]
                }
            ]
        },
        {
            testname: "mergeChunks",
            command: {mergeChunks: "test.x", bounds: [{i : 0}, {i : 5}]},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "test", collection: "x"}, actions: ["mergeChunks"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "moveChunk",
            command: {moveChunk: "test.x"},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "test", collection: "x"}, actions: ["moveChunk"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "movePrimary",
            command: {movePrimary: "x"},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "x", collection: ""}, actions: ["movePrimary"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "netstat",
            command: {netstat: "x"},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["netstat"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "ping",
            command: {ping: 1},
            testcases: [
                { runOnDb: adminDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: firstDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: secondDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] }
            ]
        },
        {
            testname: "profile",  
            command: {profile: 0},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_dbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: ""}, actions: ["enableProfiler"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_dbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: ""}, actions: ["enableProfiler"] }
                    ]
                }
            ]
        },
        {
            testname: "renameCollection_sameDb",
            command: {renameCollection: firstDbName + ".x", to: firstDbName + ".y"},
            setup: function (db) { db.getSisterDB(firstDbName).x.save( {} ); },
            teardown: function (db) {
                db.getSisterDB(firstDbName).x.drop();
                db.getSisterDB(firstDbName).y.drop();
            },
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_writeDbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: ""}, actions: ["renameCollectionSameDB"] }
                    ]
                },
                /* SERVER-11085
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
                */
            ]
        },
        {
            testname: "renameCollection_twoDbs",
            command: {renameCollection: firstDbName + ".x", to: secondDbName + ".y"},
            setup: function (db) {
                db.getSisterDB(firstDbName).x.save( {} );
                db.getSisterDB(firstDbName).getLastError();
                db.getSisterDB(adminDbName).runCommand({movePrimary: firstDbName, to: shard0name});
                db.getSisterDB(adminDbName).runCommand({movePrimary: secondDbName, to: shard0name});
            },
            teardown: function (db) {
                db.getSisterDB(firstDbName).x.drop();
                db.getSisterDB(secondDbName).y.drop();
            },
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {readWriteAnyDatabase: 1, root: 1, __system: 1},
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find", "dropCollection"] },
                        { resource: {db: secondDbName, collection: "y"}, actions: ["insert", "createIndex"] }
                    ]
                },
                /* SERVER-11085
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
                */
            ]
        },
        {
            testname: "reIndex",
            command: {reIndex: "x"},
            setup: function (db) { db.x.save( {} ); },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_dbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["reIndex"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_dbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["reIndex"] }
                    ]
                }
            ]
        },
        {
            testname: "removeShard",
            command: {removeShard: "x"},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["removeShard"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "repairDatabase",
            command: {repairDatabase: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {
                        dbAdminAnyDatabase: 1,
                        hostManager: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {db: adminDbName, collection: ""}, actions: ["repairDatabase"] }
                    ]
                },
                {
                    runOnDb: firstDbName,
                    rolesAllowed: {
                        dbAdmin: 1,
                        dbAdminAnyDatabase: 1,
                        hostManager: 1,
                        clusterAdmin: 1,
                        dbOwner: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: ""}, actions: ["repairDatabase"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: {
                        dbAdminAnyDatabase: 1,
                        hostManager: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: ""}, actions: ["repairDatabase"] }
                    ]
                }
            ]
        },
        {
            testname: "replSetElect",
            command: {replSetElect: 1},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetElect"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetFreeze",
            command: {replSetFreeze: "x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetFreeze"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetFresh",
            command: {replSetFresh: "x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetFresh"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetGetRBID",
            command: {replSetGetRBID: 1},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetGetRBID"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetGetStatus",
            command: {replSetGetStatus: 1},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {
                        clusterMonitor: 1,
                        clusterManager: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetGetStatus"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetHeartbeat",
            command: {replSetHeartbeat: 1},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetHeartbeat"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetInitiate",
            command: {replSetInitiate: "x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetInitiate"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetMaintenance",
            command: {replSetMaintenance: "x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetMaintenance"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetReconfig",
            command: {replSetReconfig: "x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetReconfig"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetStepDown",
            command: {replSetStepDown: "x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetStepDown"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "replSetSyncFrom",
            command: {replSetSyncFrom: "x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["replSetSyncFrom"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "resetError",
            command: {resetError: 1},
            testcases: [
                { runOnDb: adminDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: firstDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: secondDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] }
            ]
        },
        {
            testname: "resync",
            command: {resync: 1},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {
                        hostManager: 1,
                        clusterManager: 1,
                        clusterAdmin: 1,
                        root: 1,
                        __system: 1
                    },
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["resync"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "serverStatus",
            command: {serverStatus: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["serverStatus"] }
                    ]
                },
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["serverStatus"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["serverStatus"] }
                    ]
                }
            ]
        },
        {
            testname: "setParameter",
            command: {setParameter: 1, quiet: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["setParameter"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "setShardVersion",
            command: {setShardVersion: 1},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["setShardVersion"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "shardCollection",
            command: {shardCollection: "test.x"},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "test", collection: "x"}, actions: ["shardCollection"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "shardingState",
            command: {shardingState: 1},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["shardingState"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "shutdown",
            command: {shutdown: 1},
            testcases: [
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "split",
            command: {split: "test.x"},
            skipStandalone: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "test", collection: "x"}, actions: ["split"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "splitChunk",
            command: {splitChunk: "test.x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "test", collection: "x"}, actions: ["splitChunk"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "splitVector",
            command: {splitVector: "test.x"},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "test", collection: "x"}, actions: ["splitVector"] }
                    ],
                    expectFail: true
                },
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "test", collection: "x"}, actions: ["splitVector"] }
                    ],
                    expectFail: true
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_clusterManager,
                    requiredPrivileges: [
                        { resource: {db: "test", collection: "x"}, actions: ["splitVector"] }
                    ],
                    expectFail: true
                }
            ]
        },
        {
            testname: "storageDetails",
            command: {storageDetails: "x", analyze: "diskStorage"},
            skipSharded: true,
            setup: function (db) { db.x.save( {} ); },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_dbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["storageDetails"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_dbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["storageDetails"] }
                    ]
                }
            ]
        },
        {
            testname: "text",
            command: {text: "x"},
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_readWrite,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["find"] }
                    ],
                    expectFail: true
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_readWriteAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["find"] }
                    ],
                    expectFail: true
                }
            ]
        },
        {
            testname: "top",
            command: {top: 1},
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_monitoring,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["top"] }
                    ]
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "touch",
            command: {touch: "x", data: true, index: false},
            skipSharded: true,
            setup: function (db) { db.x.save( {} ); },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["touch"] }
                    ]
                },
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["touch"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_hostManager,
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["touch"] }
                    ]
                }
            ]
        },
        {
            testname: "unsetSharding",
            command: {unsetSharding: "x"},
            skipSharded: true,
            testcases: [
                {
                    runOnDb: adminDbName,
                    rolesAllowed: {__system: 1},
                    requiredPrivileges: [
                        { resource: {cluster: true}, actions: ["unsetSharding"] }
                    ],
                    expectFail: true
                },
                { runOnDb: firstDbName, rolesAllowed: {} },
                { runOnDb: secondDbName, rolesAllowed: {} }
            ]
        },
        {
            testname: "validate",
            command: {validate: "x"},
            setup: function (db) { db.x.save( {} ); },
            teardown: function (db) { db.x.drop(); },
            testcases: [
                {
                    runOnDb: firstDbName,
                    rolesAllowed: roles_dbAdmin,
                    requiredPrivileges: [
                        { resource: {db: firstDbName, collection: "x"}, actions: ["validate"] }
                    ]
                },
                {
                    runOnDb: secondDbName,
                    rolesAllowed: roles_dbAdminAny,
                    requiredPrivileges: [
                        { resource: {db: secondDbName, collection: "x"}, actions: ["validate"] }
                    ]
                }
            ]
        },
        {
            testname: "whatsmyuri",
            command: {whatsmyuri: 1},
            testcases: [
                { runOnDb: adminDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: firstDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] },
                { runOnDb: secondDbName, rolesAllowed: roles_all, requiredPrivileges: [ ] }
            ]
        }
    ],


    /************* SHARED TEST LOGIC ****************/

    /**
     * Returns true if conn is a connection to mongos,
     * and false otherwise.
     */
    isMongos: function(conn) {
        var res = conn.getDB("admin").runCommand({isdbgrid: 1});
        return (res.ok == 1 && res.isdbgrid == 1);
    },

    /**
     * Runs a single test object from the tests array above.
     * The actual implementation must be provided in "impls".
     *
     * Parameters:
     *   conn -- a connection to either mongod or mongos
     *   t -- a single test object from the tests array above
     *
     * Returns:
     *  An array of strings. Each string in the array reports
     *  a particular test error.
     */
    runOneTest: function(conn, t, impls) {
        jsTest.log("Running test: " + t.testname);

        // some tests shouldn't run in a sharded environment
        if (t.skipSharded && this.isMongos(conn)) {
            return [];
        }
        // others shouldn't run in a standalone environment
        if (t.skipStandalone && !this.isMongos(conn)) {
            return [];
        }

        return impls.runOneTest(conn, t);
    },

    setup: function(conn, t, runOnDb) {
        var adminDb = conn.getDB(adminDbName);
        if (t.setup) {
            adminDb.auth("admin", "password");
            t.setup(runOnDb);
            runOnDb.getLastError();
            adminDb.logout();
        }
    },

    teardown: function(conn, t, runOnDb) {
        var adminDb = conn.getDB(adminDbName);
        if (t.teardown) {
            adminDb.auth("admin", "password");
            t.teardown(runOnDb);
            runOnDb.getLastError();
            adminDb.logout();
        }
    },

    /**
     * Top-level test runner
     */
    runTests: function(conn, impls) {

        // impls must provide implementations of a few functions
        assert("createUsers" in impls);
        assert("runOneTest" in impls);

        impls.createUsers(conn);

        var failures = [];

        for (var i = 0; i < this.tests.length; i++) {
            res = this.runOneTest(conn, this.tests[i], impls);
            failures = failures.concat(res);
        }

        failures.forEach(function(i) { jsTest.log(i) });
        assert.eq(0, failures.length);
    }

}
