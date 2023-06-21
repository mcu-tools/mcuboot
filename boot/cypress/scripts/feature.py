"""
Copyright 2023 Cypress Semiconductor Corporation (an Infineon company)
or an affiliate of Cypress Semiconductor Corporation. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""


import sys
import json
import click

def load_json(file_path):
    """
    Loads JSON from file.
    """

    data_json = None

    try:
        with open(file_path, encoding="utf-8") as file:
            data_json = json.load(file)

    except FileNotFoundError:
        print(f'\nERROR: Cannot find {file_path}')
        sys.exit(-1)

    return data_json


class FieldsValidator:
    """
        Validation of required fields and their cross-dependencies.
    """

    @staticmethod
    def validate(feature_json, properties_json):
        """
            Check 'target' and properties of a platform.
        """
        p_target = properties_json.get('target')
        if p_target is None:
            raise AttributeError('Field "target" must be present in platform_properties.json')

        f_target = feature_json.get('target')
        if f_target is None:
            raise AttributeError('Field "target" must be present in a feature_config.json')

        if f_target not in p_target:
            raise AttributeError('Target in feature config is not correct.'
                                    ' It must be among the target list of platform_properties.json')

        f_security_setup = feature_json.get('security_setup')
        p_security_setup = properties_json.get('security_setup')

        if f_security_setup:

            if p_security_setup is None:
                raise AttributeError("This platform doesn't have any 'secure_setup' features")

            if f_security_setup.get('hw_rollback_prot'):
                if p_security_setup.get('hw_rollback_prot') is None:
                    raise AttributeError("This platform doesn't have HW anti roll-back counter")

            if f_security_setup.get('hw_crypto_acceleration'):
                if p_security_setup.get('hw_crypto_acceleration') is None:
                    raise AttributeError("The platform doesn't support HW crypto acceleration")

            if f_security_setup.get('validate_upgrade').get('value') is False:
                raise AttributeError("Deactivation of image validation during the upgrade \
                                        process isn't implemented yet")


class FeatureProcessor:

    """
        The general handler of all needed fields and filling the new mk-file.
    """

    settings_dict = {
        'validate_boot'             :   'MCUBOOT_SKIP_IMAGE_VALIDATION',
        'validate_upgrade'          :   'MCUBOOT_SKIP_UPGRADE_VALIDATION',
        'dependency_check'          :   'MCUBOOT_DEPENDENCY_CHECK',
        'serial_logging'            :   'MCUBOOT_LOG_LEVEL',
        'hw_rollback_prot'          :   'USE_HW_ROLLBACK_PROT',
        'hw_crypto_acceleration'    :   "USE_CRYPTO_HW",
        'sw_downgrade_prev'         :   'USE_SW_DOWNGRADE_PREV',
        'ram_app_staging'           :   'USE_STAGE_RAM_APPS',
        'xip'                       :   'USE_XIP',
        'image_encryption'          :   'ENC_IMG',
        'fault_injection_hardening' :   'FIH_PROFILE_LEVEL',
        'combine_hex'               :   'COMBINE_HEX',
        'hw_key'                    :   'USE_HW_KEY'
    }

    debug_level_dict = {
        'off'       :   '_OFF',
        'error'     :   '_ERROR',
        'warning'   :   '_WARNING',
        'info'      :   '_INFO',
        'debug'     :   '_DEBUG'
    }

    fih_level_dict = {
        'off'       :   'OFF',
        'low'       :   'LOW',
        'medium'    :   'MEDIUM',
        'high'      :   'HIGH'
    }

    def __init__(self, output_name):
        self.out_f = output_name

    @staticmethod
    def generate_header_guard():
        """
            Print header line at the begining of a mk-file
        """
        guard_lines = ('# AUTO-GENERATED FILE, DO NOT EDIT.'
                        ' ALL CHANGES WILL BE LOST! #\n\n')

        return guard_lines

    @staticmethod
    def insert_res(val_to_check) -> str:
        """
            Simlpe check result and return the string with value.
        """
        return f' := {1 if val_to_check else 0}\n'

    @staticmethod
    def insert_inverted_res(val_to_check) -> str:
        """
            Simlpe check result and return the string with inverted value.
        """
        return f' := {0 if val_to_check else 1}\n'

    def __prnt_dict_primitive_key(self, dict_feature_config, settings_dict_key, f_out):
        """
            Print kyes of 'feature_config' with bool type of 'value'
        """
        val = dict_feature_config.get(settings_dict_key).get('value')

        if isinstance(val, bool):

            # invert because variable use 'skip' command
            need_invertion = set(("validate_boot", "validate_upgrade"))

            f_out.write(self.settings_dict[settings_dict_key])

            if settings_dict_key not in need_invertion:
                f_out.write(FeatureProcessor.insert_res(val))
            else:
                f_out.write(FeatureProcessor.insert_inverted_res(val))


    def __gen_fih_level(self, fih_value):
        """
            Print only FIH_
        """
        res = f"{self.settings_dict['fault_injection_hardening']} ?= "\
              f"{self.fih_level_dict[fih_value]}\n"

        return res

    def __gen_debug_level(self, logging_value):
        """
            Print only MCUBOOT_LOG_LEVEL
        """
        param_txt = self.settings_dict['serial_logging']
        res_str = f"{param_txt} ?= {param_txt}{self.debug_level_dict[logging_value]}\n"

        return res_str


    def __handle_dictionary(self, f_dict, f_out):
        """
            Handle any dictionary of 'feature_config'
        """
        dont_print_list = set(("validation_key", "version", "description", "target"))

        for k in f_dict:

            if k not in dont_print_list:
                self.__prnt_dict_primitive_key(f_dict, k, f_out)

            if k == 'fault_injection_hardening':
                f_out.write(self.__gen_fih_level(f_dict.get(k).get("value")))

            if k == 'serial_logging':
                f_out.write(self.__gen_debug_level(f_dict.get(k).get("value")))


    def make_file_generate(self, feature_json):
        """
            Processing all keys and creation of a mk-file
        """

        with open(self.out_f, "w", encoding='UTF-8') as f_out:
            f_out.write(FeatureProcessor.generate_header_guard())

            f_security_setup_dict = feature_json.get('security_setup')

            # handling of 'security_setup' section
            if f_security_setup_dict:
                self.__handle_dictionary(f_security_setup_dict, f_out)

            self.__handle_dictionary(feature_json, f_out)


@click.group()
def cli():
    """
        Feature config parser to run from CLI
    """

@cli.command()
@click.option('-f', '--feature_config', required=True,
              help='feature configuration file path')
@click.option('-p', '--platform_properties', required=True,
              help='platform properties file path')
@click.option('-n', '--output_name', required=True,
              help='the name of the make file that will be generated')


def run(feature_config, platform_properties, output_name):
    """
        The main CLI command to run mk-file generation
    """

    feature_config_json = load_json(feature_config)
    platform_properties_json = load_json(platform_properties)

    FieldsValidator.validate(feature_config_json, platform_properties_json)

    fprocessor = FeatureProcessor(output_name)
    fprocessor.make_file_generate(feature_config_json)


if __name__ == '__main__':
    cli()
