import os
import buildscripts.utils

basicFiles = [ "jsapi.c" , 
               "jsarena.c" ,
               "jsarray.c" , 
               "jsatom.c" ,
               "jsbool.c" ,
               "jscntxt.c" ,
               "jsdate.c" ,
               "jsdbgapi.c" ,
               "jsdhash.c" ,
               "jsdtoa.c" ,
               "jsemit.c" ,
               "jsexn.c" ,
               "jsfun.c" ,
               "jsgc.c" ,
               "jshash.c" ,
               "jsiter.c" ,
               "jsinterp.c" ,
               "jslock.c" ,
               "jslog2.c" ,
               "jslong.c" ,
               "jsmath.c" ,
               "jsnum.c" ,
               "jsobj.c" ,
               "jsopcode.c" ,
               "jsparse.c" ,
               "jsprf.c" ,
               "jsregexp.c" ,
               "jsscan.c" ,
               "jsscope.c" ,
               "jsscript.c" ,
               "jsstr.c" ,
               "jsutil.c" ,
               "jsxdrapi.c" ,
               "jsxml.c" ,
               "prmjtime.c" ]

root = "third_party/js-1.7"

def r(x):
    return "%s/%s" % ( root , x )

def configure( env , fileLists , options ):
    if not options["usesm"]:
        return

    if options["windows"]:
        env.Append( CPPDEFINES=[ "XP_WIN" ] )
    else:
        env.Append( CPPDEFINES=[ "XP_UNIX" ] )

    env.Append( CPPPATH=[root] )

    myenv = env.Clone()
    myenv.Append( CPPDEFINES=[ "JSFILE" , "EXPORT_JS_API" , "JS_C_STRINGS_ARE_UTF8" ] )
    myenv["CPPFLAGS"] = myenv["CPPFLAGS"].replace( "-Werror" , "" )

    if options["windows"]:
        myenv["CPPFLAGS"] = myenv["CPPFLAGS"].replace( "/TP" , "" )
    

    if os.sys.platform.startswith( "linux" ) or os.sys.platform == "darwin":
        myenv["CPPDEFINES"] += [ "HAVE_VA_COPY" , "VA_COPY=va_copy" ]

    fileLists["scriptingFiles"] += [ myenv.Object(root + "/" + f) for f in basicFiles ]

    jskwgen = str( myenv.Program( r("jskwgen") , [ r("jskwgen.c") ] )[0] )
    jscpucfg = str( myenv.Program( r("jscpucfg") , [ r("jscpucfg.c") ] )[0] )

    def buildAutoFile( target , source , env ):
        outFile = str( target[0] )

        cmd = str( source[0] )
        if options["nix"]:
            cmd = "./" + cmd

        output = buildscripts.utils.execsys( cmd )[0]
        output = output.replace( '\r' , '\n' )
        out = open( outFile , 'w' )
        out.write( output )
        return None

    autoBuilder = myenv.Builder( action = buildAutoFile , suffix = '.h')

    env.Append( BUILDERS={ 'Auto' : autoBuilder } )
    env.Auto( r("jsautokw.h") , [ jskwgen ] )
    env.Auto( r("jsautocfg.h") , [ jscpucfg ] )
