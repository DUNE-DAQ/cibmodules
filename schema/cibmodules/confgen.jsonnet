// This is the configuration schema for cibmodules


local moo = import "moo.jsonnet";

local nc = moo.oschema.numeric_constraints;

local stypes = import "daqconf/types.jsonnet";
local types = moo.oschema.hier(stypes).dunedaq.daqconf.types;

local sboot = import "daqconf/bootgen.jsonnet";
local bootgen = moo.oschema.hier(sboot).dunedaq.daqconf.bootgen;

//local cibm = import "cibmodules/cibmodule.jsonnet";
//local cibmod = moo.oschema.hier(cibm).dunedaq.cibmodules.cibmodule;

local ns = "dunedaq.cibmodules.confgen";
local s = moo.oschema.schema(ns);


local cs = {

  cib_hsi_inst: s.record("cib_hsi_inst", [
    	s.field("trigger"	,types.int4, default=0, 					doc='Which CIB trigger is mapped by this instance'),
  		s.field("host" 	 	,types.host, default='localhost',			doc='Host where this HSI app instance will run'),
  		s.field("port" 	 	,types.host, default='8991',			        doc='Port where this HSI app instance will run'),
  		s.field("cib_host"	,types.host, default='np04-iols-cib-01', 	doc='CIB endpoint host'),
  		s.field("cib_port"	,types.port, default=8992, 					doc='CIB endpoint port'),  	
  ]),

	cib_seq : s.sequence("cib_hsi_instances",	self.cib_hsi_inst, doc="list of CIB HSI instances"),

    cibmodules_gen: s.record("cibmodules_gen", [
        s.field("boot", 			bootgen.boot, 		default=bootgen.boot, 			doc="Boot parameters"),
 	    s.field( "cib_num_modules", types.int4, default=2, doc='Number of modules to be instantiated. Default is 2 (one per periscope)'),
		s.field( "cib_instances",	self.cib_seq , default=[], doc="List of configurations for each instance"),
    ]),
};


stypes + sboot + moo.oschema.sort_select(cs, ns)
