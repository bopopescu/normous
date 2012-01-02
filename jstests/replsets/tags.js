
function myprint( x ) {
    print( "tags output: " + x );
}

var num = 5;
var host = getHostName();
var name = "tags";

var replTest = new ReplSetTest( {name: name, nodes: num, startPort:31000} );
var nodes = replTest.startSet();
var port = replTest.ports;
replTest.initiate({_id : name, members :
        [
            {_id:0, host : host+":"+port[0], tags : {"server" : "0", "dc" : "ny", "ny" : "1", "rack" : "ny.rk1"}},
            {_id:1, host : host+":"+port[1], tags : {"server" : "1", "dc" : "ny", "ny" : "2", "rack" : "ny.rk1"}},
            {_id:2, host : host+":"+port[2], tags : {"server" : "2", "dc" : "ny", "ny" : "3", "rack" : "ny.rk2", "2" : "this"}},
            {_id:3, host : host+":"+port[3], tags : {"server" : "3", "dc" : "sf", "sf" : "1", "rack" : "sf.rk1"}},
            {_id:4, host : host+":"+port[4], tags : {"server" : "4", "dc" : "sf", "sf" : "2", "rack" : "sf.rk2"}},
        ],
        settings : {
            getLastErrorModes : {
                "important" : {"dc" : 2, "server" : 3},
                "a machine" : {"server" : 1}
            }
        }});

var master = replTest.getMaster();

var config = master.getDB("local").system.replset.findOne();

printjson(config);
var modes = config.settings.getLastErrorModes;
assert.eq(typeof modes, "object");
assert.eq(modes.important.dc, 2);
assert.eq(modes.important.server, 3);
assert.eq(modes["a machine"]["server"], 1);

config.version++;
config.members[1].priority = 1.5;
config.members[2].priority = 2;
modes.rack = {"sf" : 1};
modes.niceRack = {"sf" : 2};
modes["a machine"]["2"] = 1;
modes.on2 = {"2" : 1}

try {
    master.getDB("admin").runCommand({replSetReconfig : config});
}
catch(e) {
    myprint(e);
}

replTest.awaitReplication();

myprint("primary should now be 2");
master = replTest.getMaster();
config = master.getDB("local").system.replset.findOne();
printjson(config);

modes = config.settings.getLastErrorModes;
assert.eq(typeof modes, "object");
assert.eq(modes.important.dc, 2);
assert.eq(modes.important.server, 3);
assert.eq(modes["a machine"]["server"], 1);
assert.eq(modes.rack["sf"], 1);
assert.eq(modes.niceRack["sf"], 2);

myprint("bridging");
replTest.bridge();
myprint("bridge 1");
replTest.partition(0, 3);
myprint("bridge 2");
replTest.partition(0, 4);
myprint("bridge 3");
replTest.partition(1, 3);
myprint("bridge 4");
replTest.partition(1, 4);
myprint("bridge 5");
replTest.partition(2, 3);
myprint("bridge 6");
replTest.partition(2, 4);
myprint("bridge 7");
replTest.partition(3, 4);
myprint("done bridging");

myprint("test1");
myprint("2 should be primary");
master = replTest.getMaster();

printjson(master.getDB("admin").runCommand({replSetGetStatus:1}));

var timeout = 20000;

master.getDB("foo").bar.insert({x:1});
var result = master.getDB("foo").runCommand({getLastError:1,w:"rack",wtimeout:timeout});
printjson(result);
assert.eq(result.err, "timeout");

replTest.unPartition(1,4);

myprint("test2");
master.getDB("foo").bar.insert({x:1});
result = master.getDB("foo").runCommand({getLastError:1,w:"rack",wtimeout:timeout});
printjson(result);
assert.eq(result.err, null);

myprint("test3");
result = master.getDB("foo").runCommand({getLastError:1,w:"niceRack",wtimeout:timeout});
printjson(result);
assert.eq(result.err, "timeout");

replTest.unPartition(3,4);

myprint("test4");
result = master.getDB("foo").runCommand({getLastError:1,w:"niceRack",wtimeout:timeout});
printjson(result);
assert.eq(result.err, null);

myprint("non-existent w");
result = master.getDB("foo").runCommand({getLastError:1,w:"blahblah",wtimeout:timeout});
printjson(result);
assert.eq(result.code, 14830);
assert.eq(result.ok, 0);

myprint("test on2");
master.getDB("foo").bar.insert({x:1});
result = master.getDB("foo").runCommand({getLastError:1,w:"on2",wtimeout:0});
printjson(result);
assert.eq(result.err, null);

myprint("test two on the primary");
master.getDB("foo").bar.insert({x:1});
result = master.getDB("foo").runCommand({getLastError:1,w:"a machine",wtimeout:0});
printjson(result);
assert.eq(result.err, null);

myprint("test5");
master.getDB("foo").bar.insert({x:1});
result = master.getDB("foo").runCommand({getLastError:1,w:"important",wtimeout:timeout});
printjson(result);
assert.eq(result.err, null);

replTest.unPartition(1,3);

replTest.partition(2, 0);
replTest.partition(2, 1);
replTest.stop(2);

myprint("1 must become primary here because otherwise the other members will take too long timing out their old sync threads");
master = replTest.getMaster();

myprint("test6");
master.getDB("foo").bar.insert({x:1});
result = master.getDB("foo").runCommand({getLastError:1,w:"niceRack",wtimeout:timeout});
printjson(result);
assert.eq(result.err, null);

myprint("test on2");
master.getDB("foo").bar.insert({x:1});
result = master.getDB("foo").runCommand({getLastError:1,w:"on2",wtimeout:timeout});
printjson(result);
assert.eq(result.err, "timeout");

myprint("\n\ntags.js SUCCESS\n\n");
