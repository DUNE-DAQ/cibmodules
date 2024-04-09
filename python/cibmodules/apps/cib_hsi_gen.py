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
def get_cib_hsi_app(module_name,
                    instance_id,
                    source_id,
                    conf,         # This is a configuration coming from confgen (or fddaqconf)
                    # Not really sure what these are for, just keeping them from ctbmodules
                    QUEUE_POP_WAIT_MS=10,
                    LATENCY_BUFFER_SIZE=100000,
                    DATA_REQUEST_TIMEOUT=1000,
                    host="localhost",
                    debug=False):

    '''
    Here an entire application consisting only of one CIBModule is generated. 
    '''

    # This app should only generate a single instance of the CIBModule
    
    # Define modules

    modules = []
    lus = []
    
    # Name of this instance
    instance_name = f"{module_name}{instance_id}"
    
    # There is a caveats that we need to protect against. If more than one instance is launched on the 
    # same machine, the later instances will fail to bind to the port (already in use)
    # to minimise that we'll assign a different port per instance
    #
    # Of course, this does not account for other services reserving the same port
    port = conf.port + instance_id
    lconf = cib.Conf(cib_trigger_bit=conf.trigger, 
                     cib_host=conf.cib_host, 
                     cib_port=conf.cib_port,
                     board_config=cib.Config(sockets=cib.Sockets(receiver=cib.Receiver(host=conf.host,port=port))))

    # Should one of these be made per CIB module?
    # In principle there should be no need for more than one please
    modules += [DAQModule(name = f"{instance_name}_datahandler",
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
    modules += [DAQModule(name = instance_name, 
                          plugin = 'CIBModule',
                          conf = lconf
                     )]


    queues = [Queue(f"{instance_name}.cib_output",
                    f"{instance_name}_datahandler.raw_input",
                    "HSIFrame",
                    size=100000)
                     ]
    
    mgraph = ModuleGraph(modules, queues=queues)

    mgraph.add_fragment_producer(id = source_id, 
                                 subsystem = "HW_Signals_Interface",
                                 requests_in   = f"{instance_name}_datahandler.request_input",
                                 fragments_out = f"{instance_name}_datahandler.fragment_queue")

    mgraph.add_endpoint(f"timesync_cib", 
                        f"{instance_name}_datahandler.timesync_output", 
                        "TimeSync", 
                        Direction.OUT, 
                        is_pubsub=True, 
                        toposort=False)

    # NFB: We can have just one of these
    mgraph.add_endpoint(f"{module_name}_hsievents", f"{instance_name}.hsievents", "HSIEvent",    Direction.OUT)

    # dummy subscriber
    mgraph.add_endpoint(None, None, data_type="TimeSync", inout=Direction.IN, is_pubsub=True)

    console.log('generated CIB DAQ module')

    cib_app = App(modulegraph=mgraph, host=host, name=module_name)
    console.log(f"app {instance_name} generated")

    return cib_app
