local moo = import "moo.jsonnet";

local nc = moo.oschema.numeric_constraints;

local ns = "dunedaq.cibmodules.cibmodule";
local s = moo.oschema.schema(ns);

local cibmodule = {

    int4 :    s.number(  "int4",    "i4",          doc="A signed integer of 4 bytes"),
    uint4 :   s.number(  "uint4",   "u4",          doc="An unsigned integer of 4 bytes"),
    int8 :    s.number(  "int8",    "i8",          doc="A signed integer of 8 bytes"),
    uint8 :   s.number(  "uint8",   "u8",          doc="An unsigned integer of 8 bytes"),
    float4 :  s.number(  "float4",  "f4",          doc="A float of 4 bytes"),
    double8 : s.number(  "double8", "f8",          doc="A double of 8 bytes"),
    boolean:  s.boolean( "Boolean",                doc="A boolean"),
    string:   s.string(  "String",   		   doc="A string"),   
  
  	trigger_bit_selector: s.number('trigger_bit', dtype='i4', constraints=nc(minimum=0, maximum=1),  doc='Trigger bit. Each module should be assigned a different one.'),
  
     receiver: s.record("Receiver",  [
        s.field("timeout", self.uint8, 1000, doc="CIB Receiver Connection Timeout value (microseconds)"),
        s.field("host", self.string, "localhost"),
        s.field("port", self.uint8, 8991),
     ], doc="Calibration Interface Board Receiver Socket Configuration"),
    
    sockets: s.record("Sockets",  [
        s.field("receiver", self.receiver, self.receiver),
     ], doc="Calibration Interface Board Sockets Configuration"),

    misc: s.record("Misc",  [
        s.field("trigger_stream_enable", self.boolean, false, doc="Enable storing a separate dump of all triggers received from the CIB"),
        s.field("trigger_stream_output", self.string, "/nfs/sw/trigger/cib",doc="CIB Trigger Output Path"),
        s.field("trigger_stream_update", self.uint8, "5",doc="CIB Trigger update interval (a new file is generated at this interval)"),
    ], doc="Central Trigger Board Misc Configuration"),
    	
  
    board_config: s.record("Config",  [
			s.field("sockets", self.sockets, self.sockets),
			s.field("misc", self.misc, self.misc),
        ], doc="Calibration Interface Board Configuration Wrapper"),
  
    
    conf: s.record("Conf", [
    		s.field("cib_trigger_bit", self.trigger_bit_selector, default=0, doc="Associated trigger bit" ),
    		s.field("cib_instance", self.uint4, 0, doc="CIB instance"),
	        s.field("cib_host", self.string, "np04-iols-cib-01", doc="CIB Hostname"),
	        s.field("cib_port", self.uint4, 8992, doc="CIB Connection Port"),
	        s.field("board_config", self.board_config, self.board_config, doc="CIB board configuration"),
		], doc="CIB DAQ Module Configuration"),
};

moo.oschema.sort_select(cibmodule, ns)
