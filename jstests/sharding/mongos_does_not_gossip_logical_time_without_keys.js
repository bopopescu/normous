/**
 * Tests that mongos does not gossip logical time metadata until keys are created on the config
 * server, and that it does not block waiting for keys at startup.
 */

(function() {
    "use strict";

    load("jstests/multiVersion/libs/multi_rs.js");
    load("jstests/multiVersion/libs/multi_cluster.js");  // For restartMongoses.

    function assertContainsValidLogicalTime(res) {
        assert.hasFields(res, ["$logicalTime"]);
        assert.hasFields(res.$logicalTime, ["signature", "clusterTime"]);
        // clusterTime must be greater than the uninitialzed value.
        assert.eq(bsonWoCompare(res.$logicalTime.clusterTime, Timestamp(0, 0)), 1);
        assert.hasFields(res.$logicalTime.signature, ["hash", "keyId"]);
        // The signature must have been signed by a key with a valid generation.
        assert(res.$logicalTime.signature.keyId > NumberLong(0));
    }

    let st = new ShardingTest({shards: {rs0: {nodes: 2}}});

    // Verify there are keys in the config server eventually, since mongos doesn't block for keys at
    // startup, and that once there are, mongos sends $logicalTime with a signature in responses.
    assert.soonNoExcept(function() {
        assert(st.s.getDB("admin").system.keys.count() >= 2);

        let res = assert.commandWorked(st.s.getDB("test").runCommand({isMaster: 1}));
        assertContainsValidLogicalTime(res);

        return true;
    }, "expected keys to be created and for mongos to send signed logical times");

    // Enable the failpoint, remove all keys, and restart the config servers with the failpoint
    // still enabled to guarantee there are no keys.
    assert.commandWorked(st.configRS.getPrimary().adminCommand(
        {"configureFailPoint": "disableKeyGeneration", "mode": "alwaysOn"}));
    let res =
        st.configRS.getPrimary().getDB("admin").system.keys.remove({purpose: "SigningClusterTime"});
    assert(res.nRemoved >= 2);
    assert(st.s.getDB("admin").system.keys.count() == 0,
           "expected there to be no keys on the config server");
    st.configRS.stopSet(null /* signal */, true /* forRestart */);
    st.configRS.startSet(
        {restart: true, setParameter: {"failpoint.disableKeyGeneration": "{'mode':'alwaysOn'}"}});

    // Limit the max time between refreshes on the config server, so new keys are created quickly.
    st.configRS.getPrimary().adminCommand({
        "configureFailPoint": "maxKeyRefreshWaitTimeOverrideMS",
        "mode": "alwaysOn",
        "data": {"overrideMS": 1000}
    });

    // Mongos should restart with no problems.
    st.restartMongoses();

    // There should be no logical time metadata in mongos responses.
    res = assert.commandWorked(st.s.getDB("test").runCommand({isMaster: 1}));
    assert.throws(function() {
        assertContainsValidLogicalTime(res);
    }, [], "expected mongos to not return logical time metadata");

    // Disable the failpoint.
    assert.commandWorked(st.configRS.getPrimary().adminCommand(
        {"configureFailPoint": "disableKeyGeneration", "mode": "off"}));

    // Eventually mongos will discover the new keys, and start signing logical times.
    assert.soonNoExcept(function() {
        assertContainsValidLogicalTime(st.s.getDB("test").runCommand({isMaster: 1}));
        return true;
    }, "expected mongos to eventually start signing logical times", 60 * 1000);  // 60 seconds.

    assert(st.s.getDB("admin").system.keys.count() >= 2,
           "expected there to be at least two generations of keys on the config server");

    st.stop();
})();
