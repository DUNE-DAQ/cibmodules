# This module facilitates the generation of cibmodules DAQModules within cibmodules apps


# Set moo schema search path                                                                              
from dunedaq.env import get_moo_model_path
import moo.io
from asyncio.__main__ import console
moo.io.default_load_path = get_moo_model_path()

# Load configuration types                                                                                
import moo.otypes
moo.otypes.load_types("cibmodules/cibmodule.jsonnet")
moo.otypes.load_types('readoutlibs/readoutconfig.jsonnet')

import dunedaq.cibmodules.cibmodule as cibmodule
import dunedaq.readoutlibs.readoutconfig as rconf

from daqconf.core.app import App, ModuleGraph
from daqconf.core.daqmodule import DAQModule
#from daqconf.core.conf_utils import Endpoint, Direction
from daqconf.core.conf_utils import Direction, Queue


# This is where things are a bit different from the CTB, as the CIB has a single trigger type
# On the other hand, it has multiple instances running at the same time.
# We will want to differentiate them

def get_cib_hsi_app(
        cib_hsi,
        nickname,
        LLT_SOURCE_ID,
        HLT_SOURCE_ID,
        QUEUE_POP_WAIT_MS=10,
        LATENCY_BUFFER_SIZE=100000,
        DATA_REQUEST_TIMEOUT=1000,
):
    '''
    Here an entire application controlling one CTB board is generated. 
    '''

    # Temp variables - Remove
    HOST=ctb_hsi.host_ctb_hsi
    HLT_LIST=ctb_hsi.hlt_triggers
    BEAM_LLT_LIST=ctb_hsi.beam_llt_triggers
    CRT_LLT_LIST=ctb_hsi.crt_llt_triggers
    PDS_LLT_LIST=ctb_hsi.pds_llt_triggers
    FAKE_TRIG_1=ctb_hsi.fake_trig_1
    FAKE_TRIG_2=ctb_hsi.fake_trig_2

    console = Console()

    # Define modules

    modules = []
    lus = []
    # Prepare standard config with no additional configuration
    console.log('generating DAQ module')

    # Get default LLT and HLTs
    hlt_trig = ctb.Hlt().pod()
    beam_trig = ctb.Beam().pod()
    crt_trig = ctb.Crt().pod()
    pds_trig = ctb.Pds().pod()
    fake_triggers = ctb.Misc().pod()

    # Update LLT, HLTs with new or redefined triggers
    updated_hlt_triggers = update_triggers(updated_triggers=HLT_LIST, default_trigger_conf=hlt_trig["trigger"])
    updated_beam_triggers = update_triggers(updated_triggers=BEAM_LLT_LIST, default_trigger_conf=beam_trig["triggers"])
    updated_crt_triggers = update_triggers(updated_triggers=CRT_LLT_LIST, default_trigger_conf=crt_trig["triggers"])
    updated_pds_triggers = update_triggers(updated_triggers=PDS_LLT_LIST, default_trigger_conf=pds_trig["triggers"])

    # Accept top config level fake trigger definition
    fake_trig_1 = fake_triggers["randomtrigger_1"]
    fake_trig_2 = fake_triggers["randomtrigger_2"]
    if FAKE_TRIG_1 is not None:
        fake_trig_1 = FAKE_TRIG_1
    if FAKE_TRIG_2 is not None:
        fake_trig_2 = FAKE_TRIG_2


    modules += [DAQModule(name = nickname, 
                          plugin = 'CTBModule',
                          conf = ctb.Conf(board_config=ctb.Board_config(ctb=ctb.Ctb(misc=ctb.Misc(randomtrigger_1=fake_trig_1, randomtrigger_2=fake_trig_2),
                                HLT=ctb.Hlt(trigger=updated_hlt_triggers),
                                subsystems=ctb.Subsystems(pds=ctb.Pds(triggers=updated_pds_triggers),
                                                          crt=ctb.Crt(triggers=updated_crt_triggers),
                                                          beam=ctb.Beam(triggers=updated_beam_triggers)),
                                sockets=ctb.Sockets(receiver=ctb.Receiver(host=HOST)) 
                                )))
                             )]


    modules += [DAQModule(name = f"ctb_llt_datahandler",
                        plugin = "HSIDataLinkHandler",
                        conf = rconf.Conf(readoutmodelconf = rconf.ReadoutModelConf(source_queue_timeout_ms = QUEUE_POP_WAIT_MS, 
                                                                                    source_id=LLT_SOURCE_ID,
                                                                                    send_partial_fragment_if_available = True),
                                          latencybufferconf = rconf.LatencyBufferConf(latency_buffer_size = LATENCY_BUFFER_SIZE),
                                          rawdataprocessorconf = rconf.RawDataProcessorConf(source_id=LLT_SOURCE_ID),
                                          requesthandlerconf= rconf.RequestHandlerConf(latency_buffer_size = LATENCY_BUFFER_SIZE,
                                                                                          pop_limit_pct = 0.8,
                                                                                          pop_size_pct = 0.1,
                                                                                          source_id=LLT_SOURCE_ID,
                                                                                          # output_file = f"output_{idx + MIN_LINK}.out",
                                                                                          request_timeout_ms = DATA_REQUEST_TIMEOUT,
                                                                                          warn_about_empty_buffer = False,
                                                                                          enable_raw_recording = False)
                                             ))]
                                             
    modules += [DAQModule(name = f"ctb_hlt_datahandler",
        plugin = "HSIDataLinkHandler",
        conf = rconf.Conf(readoutmodelconf = rconf.ReadoutModelConf(source_queue_timeout_ms = QUEUE_POP_WAIT_MS,
                                                                source_id=HLT_SOURCE_ID,
                                                                send_partial_fragment_if_available = True),
                        latencybufferconf = rconf.LatencyBufferConf(latency_buffer_size = LATENCY_BUFFER_SIZE),
                        rawdataprocessorconf = rconf.RawDataProcessorConf(source_id=HLT_SOURCE_ID),
                        requesthandlerconf= rconf.RequestHandlerConf(latency_buffer_size = LATENCY_BUFFER_SIZE,
                                                                        pop_limit_pct = 0.8,
                                                                        pop_size_pct = 0.1,
                                                                        source_id=HLT_SOURCE_ID,
                                                                        # output_file = f"output_{idx + MIN_LINK}.out",
                                                                        request_timeout_ms = DATA_REQUEST_TIMEOUT,
                                                                        warn_about_empty_buffer = False,
                                                                        enable_raw_recording = False)
                        ))]

    queues = [Queue(f"{nickname}.cib_iols_output",f"cib_datahandler.raw_input","HSIFrame",f'cib_link', 100000)]

    mgraph = ModuleGraph(modules, queues=queues)
    
    mgraph.add_fragment_producer(id = LLT_SOURCE_ID, subsystem = "HW_Signals_Interface",
                                         requests_in   = f"cib_datahandler.request_input",
                                         fragments_out = f"cib_datahandler.fragment_queue")

    mgraph.add_endpoint(f"timesync_cib_iols_output", f"cib_datahandler.timesync_output", "TimeSync", Direction.OUT, is_pubsub=True, toposort=False)

    mgraph.add_endpoint("hsievents", f"{nickname}.hsievents", "HSIEvent",    Direction.OUT)

    # dummy subscriber
    mgraph.add_endpoint(None, None, data_type="TimeSync", inout=Direction.IN, is_pubsub=True)

    console.log('generated DAQ module')
    cib_app = App(modulegraph=mgraph, host=HOST, name=nickname)

    return cib_app


def get_cibmodules_app(nickname, num_cibmodules, source_id, host="localhost"):
    """
    Here the configuration for an entire daq_application instance using DAQModules from cibmodules is generated.
    """
    
    # NFB: Note that we are instantiating two separate modules
    # The main issue is how to define the host of each module? Shall we simply assume that
    # they both run on the same host and just define two different consecutive ports?
    HOST=cib_hsi.host_cib_hsi
    PORT=cib_hsi.host_cib_hsi
    
    # Grab 
    console= Console()
    
    #Define the modules
    modules = []

    # Prepare standard config with no additional configuration
    console.log('generating DAQ module')

    for i in range(num_cibmodules):
        modules += [DAQModule(name = f"{nickname}_{i}", 
                              plugin = "CIBModule", 
                              # The configuration should follow the 
                              conf = cibmodule.Conf(board_config=cibmodule.Board_config(cib=cibmodule.CIB(misc=cibmodule.Misc(trigger_stream_enable=True)
                                                                                                          sockets=cibmodule.Sockets(receiver=cibmodule.Receiver(host="127.0.0.1",
                                                                                                                                                                port=4242
                                                                                                              ))))
                                )
                    )]

    modules += [DAQModule(name = f"cib_datahandler",
                        plugin = "HSIDataLinkHandler",
                        conf = rconf.Conf(readoutmodelconf = rconf.ReadoutModelConf(source_queue_timeout_ms = QUEUE_POP_WAIT_MS, 
                                                                                    source_id=source_id,
                                                                                    send_partial_fragment_if_available = True),
                                          latencybufferconf = rconf.LatencyBufferConf(latency_buffer_size = LATENCY_BUFFER_SIZE),
                                          rawdataprocessorconf = rconf.RawDataProcessorConf(source_id=source_id),
                                          requesthandlerconf= rconf.RequestHandlerConf(latency_buffer_size = LATENCY_BUFFER_SIZE,
                                                                                          pop_limit_pct = 0.8,
                                                                                          pop_size_pct = 0.1,
                                                                                          source_id=source_id,
                                                                                          # output_file = f"output_{idx + MIN_LINK}.out",
                                                                                          request_timeout_ms = DATA_REQUEST_TIMEOUT,
                                                                                          warn_about_empty_buffer = False,
                                                                                          enable_raw_recording = False)
                                             ))]

    mgraph = ModuleGraph(modules)
    cibmodules_app = App(modulegraph = mgraph, host = host, name = nickname)

    return cibmodules_app
