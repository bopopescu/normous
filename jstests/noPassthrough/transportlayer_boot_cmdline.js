/**
 * Tests the valid combinations to start a mongod works properly
 *
 */

(function() {
    'use strict';

    var baseDir = "jstests_transportlayer_boot_cmdline";
    var dbpath = MongoRunner.dataPath + baseDir + "/";

    var m = MongoRunner.runMongod({dbpath: dbpath, transportLayer: 'legacy'});
    assert(m, 'MongoDB with transportLayer=legacy failed to start up');
    MongoRunner.stopMongod(m);

    m = MongoRunner.runMongod(
        {dbpath: dbpath, transportLayer: 'legacy', serviceExecutor: 'synchronous'});
    assert(m,
           'MongoDB with transportLayer=legacy and serviceExecutor=synchronous failed to start up');
    MongoRunner.stopMongod(m);

    m = MongoRunner.runMongod(
        {dbpath: dbpath, transportLayer: 'legacy', serviceExecutor: 'fixedForTesting'});
    assert.isnull(
        m,
        'MongoDB with transportLayer=legacy and serviceExecutor=fixedForTesting managed to startup which is an unsupported combination');
    if (m) {
        MongoRunner.stopMongod(m);
    }
}());
