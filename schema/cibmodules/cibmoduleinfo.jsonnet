local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.cibmodules.cibmoduleinfo");

local info = {

    int4 :    s.number(  "int4",    "i4",          doc="A signed integer of 4 bytes"),
    uint4 :   s.number(  "uint4",   "u4",          doc="An unsigned integer of 4 bytes"),
    int8 :    s.number(  "int8",    "i8",          doc="A signed integer of 8 bytes"),
    uint8 :   s.number(  "uint8",   "u8",          doc="An unsigned integer of 8 bytes"),
    float4 :  s.number(  "float4",  "f4",          doc="A float of 4 bytes"),
    double8 : s.number(  "double8", "f8",          doc="A double of 8 bytes"),
    boolean:  s.boolean( "Boolean",                doc="A boolean"),
    string:   s.string(  "String",                 doc="A string"),   
    choice:   s.boolean( "Choice"),

//    info: s.record("Info", [
//       s.field("total_amount",                    self.int8, doc="Total count of some discrete value we care about"),
//       s.field("amount_since_last_get_info_call", self.int4, doc="Change in this discrete value since the last report"),
//    ], doc="This record is for developer education only"),
//
   info: s.record("CIBModuleInfo", [
         s.field("num_control_messages_sent",                self.uint8,     0, doc="Number of control messages sent to CIB"),
	     s.field("num_control_responses_received",           self.uint8,     0, doc="Number of control message responses received from CIB"),
	     s.field("cib_hardware_run_status",                  self.choice,     0, doc="Run status of CIB hardware itself"),
	     s.field("cib_hardware_configuration_status",        self.choice,     0, doc="Configuration status of CIB hardware itself"),
		 s.field("cib_num_triggers_received",				 self.uint8,	0, doc="Number of ts words received from CIB"),
	     s.field("num_ts_words_received",                	 self.uint8,     0, doc="Number of ts words received from CIB"),

       s.field("sent_hsi_events_counter", 			self.uint8, 	doc="Number of sent HSIEvents so far"), 
       s.field("failed_to_send_hsi_events_counter", self.uint8, 	doc="Number of failed send attempts so far"),
       s.field("last_sent_timestamp", 				self.uint8, 	doc="Timestamp of the last sent HSIEvent"),
       s.field("last_readout_timestamp", 			self.uint8, 	doc="Timestamp of the last read HLT word"),
       s.field("average_buffer_occupancy", 			self.double8, 	doc="Average (word) occupancy of buffer in CIB firmware."),

   ], doc="Calibration Interface Board Module Information"),

};

moo.oschema.sort_select(info)
