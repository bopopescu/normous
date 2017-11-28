// Cannot implicitly shard accessed collections because unsupported use of sharded collection
// from db.eval.
// @tags: [assumes_unsharded_collection, requires_non_retryable_commands]

t = db.eval1;
t.drop();

t.save({_id: 1, name: "eliot"});
t.save({_id: 2, name: "sara"});

f = function(id) {
    return db["eval1"].findOne({_id: id}).name;
};

assert.eq("eliot", f(1), "A");
assert.eq("sara", f(2), "B");
assert.eq("eliot", db.eval(f, 1), "C");
assert.eq("sara", db.eval(f, 2), "D");
