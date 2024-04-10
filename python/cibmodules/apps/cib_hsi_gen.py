# This module facilitates the generation of WIB modules within WIB apps

# Set moo schema search path
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()
from rich.console import Console

# Load configuration types
import moo.otypes

moo.otypes.load_types('cibmodules/cibmodule.jsonnet')
moo.otypes.load_types('readoutlibs/readoutconfig.jsonnet')

# Import new types
import dunedaq.cibmodules.cibmodule as cib
import dunedaq.readoutlibs.readoutconfig as rconf


from daqconf.core.app import App, ModuleGraph
from daqconf.core.daqmodule import DAQModule

from daqconf.core.conf_utils import Direction, Queue

console = Console()

#===============================================================================
def get_cib_hsi_app(nickname,
                    instance_id,
                    source_id,
                    conf,         # This is a configuration coming from confgen (or fddaqconf)
                    # Not really sure what these are for, just keeping them from ctbmodules
                    QUEUE_POP_WAIT_MS=10,
                    LATENCY_BUFFER_SIZE=100000,
                    DATA_REQUEST_TIMEOUT=1000,
                    host="localhost"):

    '''
    Here an entire application consisting only of one CIBModule is generated. 
    '''

    # This app should only generate a single instance of the CIBModule
    
    # Define modules

    modules = []
    lus = []
    
    # Name of this instance
    name = f"{nickname}_{instance_id}"

    lconf = cib.Conf(cib_trigger_bit=conf.trigger, 
                     cib_host=conf.cib_host, 
                     cib_port=conf.cib_port,
                     board_config=cib.Board_config(sockets=cib.Sockets(receiver=cib.Receiver(host=conf.host))))

    # Should one of these be made per CIB module?
    # In principle there should be no need for more than one please
    modules += [DAQModule(name = f"{name}_datahandler",
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

        
    # Customize the configuration with the parts that are specific to this instance
    # hosts and ports
    # Assign a unique name
    modules += [DAQModule(name = name, 
                          plugin = 'CIBModule',
                          conf = lconf
                     )]


    queues = [Queue(f"{name}.cib_output",
                    f"{name}_datahandler.raw_input",
                    "HSIFrame",
                    size=100000)
                     ]
    
    mgraph = ModuleGraph(modules, queues=queues)

    mgraph.add_fragment_producer(id = source_id, 
                                 subsystem = "HW_Signals_Interface",
                                 requests_in   = f"{name}_datahandler.request_input",
                                 fragments_out = f"{name}_datahandler.fragment_queue")

    mgraph.add_endpoint(f"timesync_cib", 
                        f"{name}_datahandler.timesync_output", 
                        "TimeSync", 
                        Direction.OUT, 
                        is_pubsub=True, 
                        toposort=False)

    # NFB: Can I have just one of these
    mgraph.add_endpoint(f"{nickname}_hsievents", f"{name}.hsievents", "HSIEvent",    Direction.OUT)

    # dummy subscriber
    mgraph.add_endpoint(None, None, data_type="TimeSync", inout=Direction.IN, is_pubsub=True)

    console.log('generated CIB DAQ module')

    cib_app = App(modulegraph=mgraph, host=host, name=nickname)
    console.log('app {nickname} generated')

    return cib_app
