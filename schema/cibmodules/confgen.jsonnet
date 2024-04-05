// This is the configuration schema for cibmodules


//local sdc = import "daqconf/confgen.jsonnet";
//local daqconf = moo.oschema.hier(sdc).dunedaq.daqconf.confgen;

local moo = import "moo.jsonnet";

local stypes = import "daqconf/types.jsonnet";
local types = moo.oschema.hier(stypes).dunedaq.daqconf.types;

local sboot = import "daqconf/bootgen.jsonnet";
local bootgen = moo.oschema.hier(sboot).dunedaq.daqconf.bootgen;

local cibmod = import "cibmodules/cibmodule.jsonnet";
local cibgen = moo.oschema.hier(cibmod).dunedaq.cibmodules.cibmodule;

local ns = "dunedaq.cibmodules.confgen";
local s = moo.oschema.schema(ns);


local cs = {

	 cibmod: s.record('cibmod', [
		s.field('wibserver',    self.wiblist,               default=[],          doc='TESTSTAND tcp://192.168.121.1:1234'),
    s.field('protowib',     self.wiblist,               default=[],          doc='TESTSTAND 192.168.121.1'),
    s.field('host_wib',     types.host,                 default='localhost', doc='Host to run the WIB sw app on'),
    s.field('gain',         self.gain_selector,         default=0,           doc='Channel gain selector: 14, 25, 7.8, 4.7 mV/fC (0 - 3)'),
    s.field('shaping_time', self.shaping_time_selector, default=3,           doc='Channel peak time selector: 1.0, 0.5, 3, 2 us (0 - 3)'),
    s.field('baseline',     self.baseline_selector,     default=2,           doc='Baseline selector: 0 (900 mV), 1 (200 mV), 2 (200 mV collection, 900 mV induction)'),
    s.field('pulse_dac',    self.pulse_dac_selector,    default=0,           doc='Pulser DAC setting [0-63]'),
    s.field('pulser',       types.flag,                 default=false,       doc="Switch to enable pulser"),
    s.field('gain_match',   types.flag,                 default=true,        doc="Switch to enable gain matching for pulser amplitude"),
    s.field('buffering',    self.buffering_selector,    default=0,           doc='0 (no buffer), 1 (se buffer), 2 (sedc buffer)'),
    s.field('detector_type',self.detector_type_selector,default=0,           doc='Detector type: 0 (use WIB default), 1 (upper APA), 2 (lower APA), 3 (CRP)'),
  ]),
	

	cibmodseq; s.sequence('cibs',self.cibmod, doc="Multiple CIB modules"),
	
    cibmodules_gen: s.record("cibmodules_gen", [
        s.field("boot", bootgen.boot, default=bootgen.boot, doc="Boot parameters"),
        s.field("num_cib_modules", types.int4, default=2, doc="Number of CIB modules"),
        s.field("cib_modules",self.cibmodseq, )
        
    ]),
};


// Output a topologically sorted array.
//sdc + moo.oschema.sort_select(cs, ns)
stypes + sboot + moo.oschema.sort_select(cs, ns)
