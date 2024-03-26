# This module facilitates the generation of cibmodules DAQModules within cibmodules apps


# Set moo schema search path                                                                              
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()

# Load configuration types                                                                                
import moo.otypes
moo.otypes.load_types("cibmodules/cibmodule.jsonnet")

import dunedaq.cibmodules.cibmodule as cibmodule

from daqconf.core.app import App, ModuleGraph
from daqconf.core.daqmodule import DAQModule
#from daqconf.core.conf_utils import Endpoint, Direction

def get_cibmodules_app(nickname, num_cibmodules, some_configured_value, host="localhost"):
    """
    Here the configuration for an entire daq_application instance using DAQModules from cibmodules is generated.
    """

    modules = []

    for i in range(num_cibmodules):
        modules += [DAQModule(name = f"nickname{i}", 
                              plugin = "CIBModule", 
                              conf = cibmodule.Conf(some_configured_value = some_configured_value
                                )
                    )]

    mgraph = ModuleGraph(modules)
    cibmodules_app = App(modulegraph = mgraph, host = host, name = nickname)

    return cibmodules_app
