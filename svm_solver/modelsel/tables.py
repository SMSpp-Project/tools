#!/usr/bin/env python3
"""Tables of the study on the model selection of an SVM, out of raw.tsv.

    tables.py <raw.tsv> [section ...]

For each section, one row per data set and kernel and one column per way of
doing the operation (Solver and mode): the median of the repetitions, in
seconds. Below the table, the noise, i.e., for each variant run twice the
relative difference of the two times (the variant compared with itself), and
the runs that failed or timed out. A time taken while the load of the machine
was above the threshold of the campaign is marked with '*'."""

import sys
import statistics as st
from collections import defaultdict

IDLE = 8.0

def main():
    rows = [ l.rstrip( "\n" ).split( "\t" ) for l in open( sys.argv[ 1 ] ) ]
    head , rows = rows[ 0 ] , rows[ 1: ]
    col = { h : i for i , h in enumerate( head ) }
    want = sys.argv[ 2: ] or sorted( { r[ col[ "section" ] ] for r in rows } )

    for sec in want:
        cell = defaultdict( list )
        loaded = set()
        bad = []
        variants = []
        keys = []
        for r in rows:
            if r[ col[ "section" ] ] != sec:
                continue
            key = ( r[ col[ "data" ] ] , r[ col[ "kernel" ] ] )
            var = r[ col[ "solver" ] ] + ":" + r[ col[ "mode" ] ]
            if r[ col[ "jobs" ] ] != "1":
                var += "@" + r[ col[ "jobs" ] ]
            if var not in variants:
                variants.append( var )
            if key not in keys:
                keys.append( key )
            if r[ col[ "rc" ] ] != "0":
                bad.append( ( key , var , r[ col[ "rc" ] ] ) )
                continue
            cell[ key , var ].append( float( r[ col[ "seconds" ] ] ) )
            if max( float( r[ col[ "load0" ] ] ) ,
                    float( r[ col[ "load1" ] ] ) ) > IDLE * 1.5:
                loaded.add( ( key , var ) )

        print( "== " + sec )
        print( "%-22s %-8s" % ( "data" , "kernel" ) +
               "".join( " %12s" % v[ :12 ] for v in variants ) )
        noise = []
        for key in keys:
            line = "%-22s %-8s" % key
            for v in variants:
                t = cell.get( ( key , v ) )
                if not t:
                    line += " %12s" % "-"
                    continue
                mark = "*" if ( key , v ) in loaded else ""
                line += " %12s" % ( "%.3f%s" % ( st.median( t ) , mark ) )
                if len( t ) == 2 and min( t ) > 0.05:
                    noise.append( abs( t[ 0 ] - t[ 1 ] ) / min( t ) )
            print( line )
        if noise:
            noise.sort()
            print( "noise (A/A, runs over 0.05 s): median %.1f%%, 90th %.1f%%, "
                   "max %.1f%% on %d pairs" %
                   ( 100 * st.median( noise ) ,
                     100 * noise[ int( 0.9 * ( len( noise ) - 1 ) ) ] ,
                     100 * noise[ -1 ] , len( noise ) ) )
        for key , v , rc in bad:
            print( "failed: %s %s %s rc=%s" % ( key[ 0 ] , key[ 1 ] , v , rc ) )
        print()

if __name__ == "__main__":
    main()
