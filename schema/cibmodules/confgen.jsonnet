// This is the configuration schema for cibmodules

local moo = import "moo.jsonnet";
local sdc = import "daqconf/confgen.jsonnet";
local daqconf = moo.oschema.hier(sdc).dunedaq.daqconf.confgen;

local ns = "dunedaq.cibmodules.confgen";
local s = moo.oschema.schema(ns);


//local cs = {
//
//    int4 :    s.number(  "int4",    "i4",          doc="A signed integer of 4 bytes"),
//    uint4 :   s.number(  "uint4",   "u4",          doc="An unsigned integer of 4 bytes"),
//    int8 :    s.number(  "int8",    "i8",          doc="A signed integer of 8 bytes"),
//    uint8 :   s.number(  "uint8",   "u8",          doc="An unsigned integer of 8 bytes"),
//    float4 :  s.number(  "float4",  "f4",          doc="A float of 4 bytes"),
//    double8 : s.number(  "double8", "f8",          doc="A double of 8 bytes"),
//    boolean:  s.boolean( "Boolean",                doc="A boolean"),
//    string:   s.string(  "String",   		   doc="A string"),   
//    monitoring_dest: s.enum(     "MonitoringDest", ["local", "cern", "pocket"]),
//
//    cibmodules: s.record("cibmodules", [
//        s.field( "some_configured_value", self.int4, default=31415, doc="A value which configures the CIBModule DAQModule instance"),
//        s.field( "num_cibmodules", self.int4, default=2, doc="A value which configures the number of instances of CIBModule"),
//    ]),
//
//    cibmodules_gen: s.record("cibmodules_gen", [
//        s.field("boot", daqconf.boot, default=daqconf.boot, doc="Boot parameters"),
//        s.field("cibmodules", self.cibmodules, default=self.cibmodules, doc="cibmodules parameters"),
//    ]),
//};
//
local cs = {

    id :    s.number(  "int4",    "i4",          doc="IoLS ID"),
    enable:  s.boolean( "Boolean",                doc="Enable this IoLS system"),
    description:   s.string(  "String",   		   doc="Description of the IoLS system"),   
    monitoring_dest: s.enum(     "MonitoringDest", ["local", "cern", "pocket"]),

    cibmodules: s.record("cibmodules", [
        s.field( "some_configured_value", self.int4, default=31415, doc="A value which configures the CIBModule DAQModule instance"),
        s.field( "num_cibmodules", self.int4, default=2, doc="A value which configures the number of instances of CIBModule"),
    ]),

    cibmodules_gen: s.record("cibmodules_gen", [
        s.field("boot", daqconf.boot, default=daqconf.boot, doc="Boot parameters"),
        s.field("cibmodules", self.cibmodules, default=self.cibmodules, doc="cibmodules parameters"),
    ]),
};


// Output a topologically sorted array.
sdc + moo.oschema.sort_select(cs, ns)
