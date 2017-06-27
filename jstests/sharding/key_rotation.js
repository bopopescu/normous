/**
 * Tests for causal consistency key rotation. In particular, tests:
 * - that a sharded cluster with no keys inserts new keys after startup.
 * - responses from servers in a sharded cluster contain a logical time object with a signature.
 * - manual key rotation is possible by deleting existing keys and restarting the cluster.
 *
 * Manual key rotation requires restarting a shard, so a persistent storage engine is necessary.
 * @tags: [requires_persistence]
 */
(function() {
    "use strict";

    let st = new ShardingTest({shards: {rs0: {nodes: 2}}, mongosWaitsForKeys: true});

    // Verify after startup there is a new key in admin.system.keys.
    jsTestLog("Verify the admin.system.keys collection after startup.");

    let startupKeys = st.s.getDB("admin").system.keys.find();
    assert(startupKeys.count() >= 2);  // Should be at least two generations of keys available.
    startupKeys.toArray().forEach(function(key, i) {
        assert.hasFields(
            key,
            ["purpose", "key", "expiresAt"],
            "key document " + i + ": " + tojson(key) + ", did not have all of the expected fields");
    });

    // Verify there is a $logicalTime with a signature in the response.
    jsTestLog("Verify a signature is included in the logical time in a response.");

    let res = assert.commandWorked(st.s.getDB("test").runCommand({isMaster: 1}));
    assert.hasFields(res, ["$logicalTime"]);
    assert.hasFields(res.$logicalTime, ["signature"]);
    assert.hasFields(res.$logicalTime.signature, ["hash", "keyId"]);

    // Verify manual key rotation.
    jsTestLog("Verify manual key rotation.");

    // Pause key generation on the config server primary.
    st.configRS.getPrimary().getDB("admin").runCommand(
        {configureFailPoint: "disableKeyGeneration", mode: "alwaysOn"});

    // Delete all existing keys.
    res =
        st.configRS.getPrimary().getDB("admin").system.keys.remove({purpose: "SigningClusterTime"});
    assert(res.nRemoved >= 2);
    assert(st.s.getDB("admin").system.keys.find().count() == 0);

    // Restart the config servers, so they will create new keys once the failpoint is disabled.
    st.configRS.stopSet(null /* signal */, true /* forRestart */);
    st.configRS.startSet(
        {restart: true, setParameter: {"failpoint.disableKeyGeneration": "{'mode':'alwaysOn'}"}});

    // Limit the max time between refreshes on the config server, so new keys are created quickly.
    st.configRS.getPrimary().adminCommand({
        "configureFailPoint": "maxKeyRefreshWaitTimeOverrideMS",
        "mode": "alwaysOn",
        "data": {"overrideMS": 1000}
    });

    // Kill and restart all shards and mongos processes so they have no keys in memory.
    st.rs0.stopSet(null /* signal */, true /* forRestart */);
    st.rs0.startSet({restart: true});
    st.restartMongos(0);

    // The shard primary should return a dummy signed logical time, because there are no keys.
    res = assert.commandWorked(st.rs0.getPrimary().getDB("test").runCommand({isMaster: 1}));
    assert.hasFields(res, ["$logicalTime", "operationTime"]);
    assert.eq(res.$logicalTime.signature.keyId, NumberLong(0));

    // Mongos shouldn't return a logical time at all.
    res = assert.commandWorked(st.s.getDB("test").runCommand({isMaster: 1}));
    assert.throws(function() {
        assert.hasFields(res, ["$logicalTime", "operationTime"]);
    }, [], "expected the mongos not to return logical time or operation time");

    // Resume key generation.
    st.configRS.getPrimary().getDB("admin").runCommand(
        {configureFailPoint: "disableKeyGeneration", mode: "off"});

    // Wait for config server primary to create new keys.
    assert.soonNoExcept(function() {
        let keys = st.s.getDB("admin").system.keys.find();
        assert(keys.count() >= 2);
        return true;
    }, "expected the config server primary to create new keys");

    st.stop();
})();
