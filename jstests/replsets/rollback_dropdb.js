/**
 * Test that a rollback of 'dropDatabase' will cause the node to go into a FATAL state
 *
 * If all data-bearing nodes in a replica set are using an ephemeral storage engine, the set will
 * not be able to survive a scenario where all data-bearing nodes are down simultaneously. In such a
 * scenario, none of the members will have any data, and upon restart will each look for a member to
 * initial sync from, so no primary will be elected. This test induces such a scenario, so cannot be
 * run on ephemeral storage engines.
 * @tags: [requires_persistence]
 */

// Sets up a replica set and grabs things for later.
var name = "rollback_dropdb";
var replTest = new ReplSetTest({name: name, nodes: 3});
var nodes = replTest.nodeList();
var conns = replTest.startSet();
replTest.initiate({
    "_id": name,
    "members": [
        {"_id": 0, "host": nodes[0], priority: 3},
        {"_id": 1, "host": nodes[1]},
        {"_id": 2, "host": nodes[2], arbiterOnly: true}
    ]
});
var a_conn = conns[0];
var b_conn = conns[1];
var AID = replTest.getNodeId(a_conn);
var BID = replTest.getNodeId(b_conn);

replTest.waitForState(replTest.nodes[0], ReplSetTest.State.PRIMARY);

// Gets master and does an initial write.
var master = replTest.getPrimary();
assert(master === conns[0], "conns[0] assumed to be master");
assert(a_conn.host === master.host, "a_conn assumed to be master");
var writeConcern = {w: 2, wtimeout: replTest.kDefaultTimeoutMS};
var options = {writeConcern: writeConcern, upsert: true};
assert.writeOK(a_conn.getDB(name).foo.insert({x: 1}, options));

// Since SERVER-29277, if there are any collections present in a database at the time of a
// dropDatabase request, we will explicitly drop each collection and wait for the collection drops
// to be replicated to a majority of nodes. However, due to the nature of this test, we will not
// have a majority of nodes to complete collection drops after shutting down the initial primary
// node A.
// Therefore, we drop the collection now on all data bearing nodes. This still leaves the database
// around for us to drop on the new primary node B (after shutting down node A).
assert.commandWorked(a_conn.getDB(name).runCommand({drop: 'foo'}, {writeConcern: writeConcern}),
                     "dropping collection failed.");

// Shuts down the master.
replTest.stop(AID);

// Drops the database which should cause FATAL when rolled back.
master = replTest.getPrimary();
assert(b_conn.host === master.host, "b_conn assumed to be master");
assert.commandWorked(b_conn.getDB(name).dropDatabase(), 'dropping database failed');
assert.eq(1,
          b_conn.getDB('local')
              .oplog.rs.find({op: 'c', ns: name + '.$cmd', 'o.dropDatabase': 1})
              .itcount(),
          'dropDatabase oplog entry not written to oplog');

// Shuts down B and brings back the original master.
replTest.stop(BID);
replTest.restart(AID);
master = replTest.getPrimary();
assert(a_conn.host === master.host, "a_conn assumed to be master");

// Does a write so that B will have to roll back.
options = {
    writeConcern: {w: 1, wtimeout: 60000},
    upsert: true
};
assert.writeOK(a_conn.getDB(name).foo.insert({x: 2}, options));

// Restarts B, which should attempt rollback but then fassert.
clearRawMongoProgramOutput();
replTest.restart(BID);
assert.soon(function() {
    return rawMongoProgramOutput().match(
        "Can't roll back drop database. Full resync will be required");
}, "B failed to fassert");

replTest.stop(BID, undefined, {allowedExitCode: MongoRunner.EXIT_ABRUPT});
replTest.stopSet();