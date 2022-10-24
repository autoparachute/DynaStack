## To download dynamoRIO: ##
	git clone https://github.com/DynamoRIO/dynamorio.git

## To build dynamoRIO: ##
	$ mkdir build && cd build
	$ cmake .. && make && make install

## To configure shadow stack: ##
	you should open dynamorio/api/samples/CMakeLists.txt 
	add " add_sample_client(shadowstack     "shadowstack.c"     "drmgr;drsyms") "	
	below " # dr_insert_mbr_instrumentation is NYI "

## To run shadow stack: ##
	cd build/
	$ ../exports/bin32/drrun -c ./api/bin/libshadowstack.so -- <program> <args>

## To better understand the source code: ##
	the function call diagram of the shadow stack program can be viewed in func_call_graph.png

