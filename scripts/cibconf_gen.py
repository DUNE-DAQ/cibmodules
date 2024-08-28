#!/usr/bin/env python3

import json
import os
import math
import sys
import glob
import rich.traceback
import shutil
from rich.console import Console
# from os.path import exists, join
from pathlib import Path
from daqconf.core.system import System
from daqconf.core.conf_utils import make_app_command_data
from daqconf.core.config_file import generate_cli_from_schema
from daqconf.core.metadata import write_metadata_file

import moo.otypes
moo.otypes.load_types('cibmodules/cibmodule.jsonnet')
#import dunedaq.cibmodules.cibmodule as cib

# Add -h as default help option
CONTEXT_SETTINGS = dict(help_option_names=['-h', '--help'])

console = Console()

# Set moo schema search path
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()

import click

@click.command(context_settings=CONTEXT_SETTINGS)
@generate_cli_from_schema('cibmodules/confgen.jsonnet', 'cibmodules_gen')
@click.option('--force-pm', default=None, type=click.Choice(['ssh', 'k8s']), help="Force process manager")
@click.option('-a', '--only-check-args', default=False, is_flag=True, help="Dry run, do not generate output files")
@click.option('-n', '--dry-run', default=False, is_flag=True, help="Dry run, do not generate output files")
@click.option('-f', '--force', default=False, is_flag=True, help="Force configuration generation - delete ")
@click.option('--debug', default=False, is_flag=True, help="Switch to get a lot of printout and dot files")
@click.argument('json_dir', type=click.Path())

def cli(
    config,
    force_pm,
    only_check_args,
    dry_run,
    force,
    debug,
    json_dir
    ):

    if only_check_args:
        return
    
    output_dir = Path(json_dir)
    if output_dir.exists():
        if dry_run:
            pass
        elif force:
            console.log(f"Removing existing {output_dir}")
            # Delete output folder if it exists
            shutil.rmtree(output_dir)
        else:
            raise RuntimeError(f"Directory {output_dir} already exists")
        

    config_data = config[0]
    config_file = config[1]
    
    console.log(f'Receiving config_data [{config_data}]')
    console.log(f'Receiving config_file [{config_file}]')
    console.log(f"\nTotal configuration for this app before any overrides: {config_data.pod()}")

    #console.log('Loading cardcontrollerapp config generator')
    #from cibmodules.boardcontrollerapp import boardcontrollerapp_gen
    console.log("Loading cibmodulesapp config generator")
    #from cibmodules import cibmodulesapp_gen
    #import cibmodules as cc
    #print(cc)
    import dunedaq.cibmodules.confgen as confgen
    moo.otypes.load_types('daqconf/bootgen.jsonnet')
    import dunedaq.daqconf.bootgen as bootgen
    moo.otypes.load_types('cibmodules/cibmodule.jsonnet')
    import dunedaq.cibmodules.cibmodule as cibmodule
    #print(str(cibmodule.Conf))
    
    boot = bootgen.boot(**config_data.boot)
    console.log(f"boot configuration object: {boot.pod()}")
    #console.log(f"cibmodule configuration object: {cibmodule.pod()}")
    #for x in config_data:
    #    print(x,config_data[x])
    
    num_cib_modules = config_data.cib_num_modules
    print(config_data.cib_num_modules)
    # For some weird reason I cannot instantiate the properties of cibmodule
    # from inside the confgen
    #print(config_data.cib_modules)

    console.log(f"Generating {num_cib_modules} instances of CIBModule")
    
    # Update with command-line options
    if force_pm is not None:
        boot.process_manager = force_pm
        console.log(f"boot.boot.process_manager set to {boot.process_manager}")

    the_system = System()
    # set the nickname to something memorable
    nickname = "cib"

    console.log('Loading cibapp config generator')
    # This is the folder structure in the cibmodule
    from cibmodules.apps import cib_hsi_gen
    
    import dunedaq.cibmodules.cibmodule as cibmodule

    # now loop over the number of instances
    # Check whether a configuration already has been defined
    cibinst = [ c for c in config_data.cib_instances ]
    num_inst_confs = len(cibinst)
    
    print(cibinst)
    #print(str(confgen.cib_hsi_instances.items()))
    
    #print(str(confgen.cib_hsi_instances[0]))
    
    
    # grab a default config
    default_config = confgen.cib_hsi_inst()
    
    #ctb_hsi = confgen.ctb_hsi(**config_data.ctb_hsi)
    #if debug: console.log(f"ctb_hsi configuration object: {ctb_hsi.pod()}")

    lconfs = confgen.cib_hsi_instances
    
    # Set default configuration for all of them
    if num_inst_confs != num_cib_modules:
        lconfs = [default_config for i in range(num_cib_modules)]
    for i in range(num_cib_modules):
        # Check i
        name = f"cib{i}"
        
        console.log(f"Generating instance {i} of CIBModule app : {name}")
        the_system.apps[name] = cib_hsi_gen.get_cib_hsi_app(
            nickname = nickname, 
            instance_id= i,
            source_id = i,
            conf = lconfs[i]
        )

    console.log('generated cib_hsi_apps')
    ####################################################################
    # Application command data generation
    ####################################################################

    from daqconf.core.conf_utils import make_app_command_data
    # Arrange per-app command data into the format used by util.write_json_files()
    app_command_datas = {
        name : make_app_command_data(the_system, app, name)
        for name,app in the_system.apps.items()
    }

    # Make boot.json config
    from daqconf.core.conf_utils import make_system_command_datas, write_json_files
    system_command_datas = make_system_command_datas(
        boot,
        the_system,
    )

    write_json_files(app_command_datas, system_command_datas, json_dir, verbose=True)

    console.log(f"cibmodules app config generated in {json_dir}")

    write_metadata_file(json_dir, "cibmodules_gen", config_file)


    return None


if __name__ == '__main__':
    try:
        cli(show_default=True, standalone_mode=True)
    except Exception as e:
        console.print_exception()
        raise SystemExit(-1)