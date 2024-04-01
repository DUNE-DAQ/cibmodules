local moo = import "moo.jsonnet";
local ns = "dunedaq.cibmodules.cibmodule";
local s = moo.oschema.schema(ns);

local types = {

    int4 :    s.number(  "int4",    "i4",          doc="A signed integer of 4 bytes"),
    uint4 :   s.number(  "uint4",   "u4",          doc="An unsigned integer of 4 bytes"),
    int8 :    s.number(  "int8",    "i8",          doc="A signed integer of 8 bytes"),
    uint8 :   s.number(  "uint8",   "u8",          doc="An unsigned integer of 8 bytes"),
    float4 :  s.number(  "float4",  "f4",          doc="A float of 4 bytes"),
    double8 : s.number(  "double8", "f8",          doc="A double of 8 bytes"),
    boolean:  s.boolean( "Boolean",                doc="A boolean"),
    string:   s.string(  "String",   		   doc="A string"),   

    // TO cibmodules DEVELOPERS: PLEASE DELETE THIS FOLLOWING COMMENT AFTER READING IT
    // The following code is an example of a configuration record
    // written in jsonnet. In the real world it would be written so as
    // to allow the relevant members of CIBModule to be configured by
    // Run Control
  
  
     receiver: s.record("Receiver",  [
        s.field("timeout", self.uint8, 1000, doc="CIB Receiver Connection Timeout value (microseconds)"),
        s.field("host", self.string, "localhost"),
        s.field("port", self.uint8, 8992),
     ], doc="Calibration Interface Board Receiver Socket Configuration"),

    monitor: s.record("Monitor",  [
        s.field("enable", self.boolean, false),
        s.field("host", self.string, "localhost"),
        s.field("port", self.uint8, 8993),
     ], doc="Calibration Interface Board  Monitor Socket Configuration"),

    statistics: s.record("Statistics",  [
        s.field("enable", self.boolean, false),
        s.field("port", self.uint8, 8994),
        s.field("updt_period", self.uint8, 1),
     ], doc="Calibration Interface Board Statistics Socket Configuration"),

    sockets: s.record("Sockets",  [
        s.field("receiver", self.receiver, self.receiver),
        s.field("monitor", self.monitor, self.monitor),
        s.field("statistics", self.statistics, self.statistics),
     ], doc="Calibration Interface Board Sockets Configuration"),
    
    timing: s.record("Timing",  [
        s.field("address", self.string, "0xF0"),
        s.field("group", self.string, "0x0"),
        s.field("triggers", self.boolean, true),
        s.field("lockout", self.string, "0x10"),
    ], doc="Calibration Interface Board Timing Configuration"),
  
  
    misc: s.record("Misc",  [
        s.field("timing", self.timing, self.timing),
        s.field("standalone_enable", self.boolean, false, doc="Enable running in standalone mode, with a free running clock"),
        s.field("trigger_stream_enable", self.boolean, false, doc="Enable storing a separate dump of all triggers received from the CIB"),
        s.field("trigger_stream_output", self.string, "/nfs/sw/trigger/cib",doc="CIB Trigger Output Path"),
        s.field("trigger_stream_update", self.uint8, "5",doc="CIB Trigger update interval"),
    ], doc="Central Trigger Board Misc Configuration"),
  
  
    cib: s.record("CIB",  [
        s.field("sockets", self.sockets, self.sockets),
        s.field("misc", self.misc, self.misc),
        ], doc="Calibration Interface Board Configuration Object"),
  	
  
    board_config: s.record("Board_config",  [
			s.field("cib", self.cib, self.cib),
        ], doc="Calibration Interface Board Configuration Wrapper"),
  
    conf: s.record("Conf", [
    
            s.field("cib_control_timeout", self.uint8, 1000,
	                doc="CIB Receiver Connection Timeout value (microseconds)"),
	
	        s.field("cib_control_port", self.uint8, 8991,
	                doc="CIB Control Connection Port"),
	
	        s.field("cib_control_host", self.string, "np04-cib-1",
	                doc="CIB Hostname"),
	           	 
	        s.field("board_config", self.board_config, self.board_config, doc="CIB board config"),
	    
                           ],
                   doc="CIB DAQ Module Configuration"),
    

};

moo.oschema.sort_select(types, ns)
