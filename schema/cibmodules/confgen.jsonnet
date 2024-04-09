// This is the configuration schema for cibmodules


local moo = import "moo.jsonnet";

local nc = moo.oschema.numeric_constraints;

local stypes = import "daqconf/types.jsonnet";
local types = moo.oschema.hier(stypes).dunedaq.daqconf.types;

local sboot = import "daqconf/bootgen.jsonnet";
local bootgen = moo.oschema.hier(sboot).dunedaq.daqconf.bootgen;

local cibm = import "cibmodules/cibmodule.jsonnet";
local cibmod = moo.oschema.hier(cibm).dunedaq.cibmodules.cibmodule;

local ns = "dunedaq.cibmodules.confgen";
local s = moo.oschema.schema(ns);


local cs = {

	cib: s.record('cib', [
		s.field('name', 	types.string, 	default="", 			doc='CIB module identifier'),
		//s.field('config', 	cibmod.Conf, 	default={}, 			doc="CIB module config"),
	], doc='A CIBModule configuration'),

	cibmodseq: s.sequence('cibs',self.cib, doc="Multiple CIB modules"),
	
    cibmodules_gen: s.record("cibmodules_gen", [
        s.field("boot", 			bootgen.boot, 		default=bootgen.boot, 			doc="Boot parameters"),
        s.field("num_cib_modules", 	types.int4, 		default=2,   					doc="Number of CIB modules"),
        s.field("cib_modules",		self.cibmodseq, 	default=[],			doc="CIB module instances"),	
    ]),
};

// Output a topologically sorted array.
//sdc + moo.oschema.sort_select(cs, ns)
stypes + sboot + moo.oschema.sort_select(cs, ns)
