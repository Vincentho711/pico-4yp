# commands/commands.py
import abc
import argparse
import asyncio
import logging
import serial_asyncio
from typing import Optional
from message_handler.message_handler import prepare_set_periodic_sampler_msg, prepare_stop_periodic_sampler_msg, prepare_execute_one_off_sampler_msg
from communications.protocol import IngressProtocol

# Access the logger from the parent script
logger = logging.getLogger(__name__)

# Abstract Command class for parsing user input
class Command(metaclass=abc.ABCMeta):
    # Class property which subclasses must provide to store the command name
    command_name: Optional[str] = None
    # Class property which subclasses must provide to store the description for the command
    command_info: Optional[str] = None
    # Class property which subclasses must provide to indicate whether the execute() function is async or not
    command_is_async: Optional[bool] = None

    # Class property shared amongst all subclasses
    # If a subclass modifies this class attribute, the change will be visible to other instances of the same subclass as well as instances of its parent class and other subclasses
    async_transport: Optional[asyncio.Transport] = None
    # Current interface used
    interface: str = "Unconnected" # Default is 'Unconnected'
    # Whether it is in streaming mode
    streaming: bool = False
    # Whether data logger has been configured
    configured: bool = False

    @classmethod
    @abc.abstractmethod
    def execute(cls, command_args: argparse.Namespace,  state: dict) -> None:
        raise NotImplementedError("Subclasses must implement the execute() method.")

    @classmethod
    def get_argument_parser(cls) -> argparse.ArgumentParser:
        # Get the argument names and create a custom usage string
        parser = argparse.ArgumentParser(description=f"{cls.command_info}")
        return parser

    # Static method is used and Command is directly referenced to ensure same behaviour
    # regardless of which subclass is calling the method
    @staticmethod
    def set_async_transport(transport: Optional[asyncio.Transport]) -> None:
        Command.async_transport = transport 

    @staticmethod
    def set_interface(interface: str) -> None:
        Command.interface = interface

    @staticmethod
    def set_streaming(streaming: bool) -> None:
        Command.streaming = streaming

    @staticmethod
    def set_configured(configured: bool) -> None:
        Command.configured = configured

class UsbConnectCommand(Command):
    command_name = "usb_connect"
    command_info = "Connect to the data logger via USB."
    command_is_async = True

    # This is an async function as data from serial is processed using callbacks
    @classmethod
    async def execute(cls, command_args: argparse.Namespace, state: dict) -> None:
        print(f"{cls.command_name} executed.")
        # Change the async transport to serial such that other execute() function can utilise it
        if cls.async_transport is None:
            serial_transport, protocol = await serial_asyncio.create_serial_connection(
                 loop = asyncio.get_event_loop(),
                 protocol_factory = IngressProtocol,
                 url = command_args.port,
                 baudrate = 115200
                 )
            cls.set_async_transport(serial_transport)
            logger.debug(f"Set async transport to '{cls.async_transport}'.")
            cls.set_interface("USB")
            logger.debug(f"Set interface to '{cls.interface}'.")
        else:
            print(f"Invalid operation : The current connectivity is set to '{cls.async_transport}'. Please disconnect from that connectivity interface before connecting via usb.")
            logger.error(f"Command.async_transport is currently set to '{cls.async_transport}'. Please disconnect from that connectivity interface before connecting via usb.")

    @classmethod
    def get_argument_parser(cls) -> argparse.ArgumentParser:
        parser = super().get_argument_parser()
        parser.add_argument("port", type=str, help="USB port of the device. E.g. '/dev/cu.usbmodem1201'.")
        # Update the usage part of the 'help' message according to the arguments specific to a command
        usage_parts = [cls.command_name]
        usage_parts.extend([f"[{arg.dest}]" for arg in parser._actions[1:]])
        parser.usage = ' '.join(usage_parts)
        return parser

class SetPeriodicSamplingCommand(Command):
    command_name = "set_periodic_sampling"
    command_info = "Start the periodic sampler with a fixed sampling frequency."
    command_is_async = False

    @classmethod
    def execute(cls, command_args: argparse.Namespace, state: dict) -> None:
        try:
            print(f"'{cls.command_name}' executed.")
            if cls.async_transport is not None:
                msg = prepare_set_periodic_sampler_msg(sampling_period=command_args.sampling_period)
                # Transport from async context is required for communicating with the underlying async low-level event loop to write to serial/TCP
                logger.debug(f"Writing set periodic sampling msg with transport '{type(cls.async_transport)}'.")
                cls.async_transport.write(msg)
                cls.set_streaming(True)
                logger.debug(f"Set streaming to '{cls.streaming}'.")
            else:
                print(f"Invalid operation : Please connect to a connectivity interface before setting up periodic sampling. E.g. Use 'usb_connect' command to connect to usb first.")
                logger.error(f"Command.async_transport has not been set to any type of Transport. Current transport : '{cls.async_transport}'.")
        except Exception as e:
            logger.exception(f"Exception in execute() : {e}")

    @classmethod
    def get_argument_parser(cls) -> argparse.ArgumentParser:
        parser = super().get_argument_parser()
        parser.add_argument("sampling_period", type=int, help="Sampling period of the periodic sampler in micro-seconds. Min = 20.")
        # Update the usage part of the 'help' message according to the arguments specific to a command
        usage_parts = [cls.command_name]
        usage_parts.extend([f"[{arg.dest}]" for arg in parser._actions[1:]])
        parser.usage = ' '.join(usage_parts)
        return parser

class StopPeriodicSamplingCommand(Command):
    command_name = "stop_periodic_sampling"
    command_info = "Stop periodic sampling on the data logger."
    command_is_async = False

    @classmethod
    def execute(cls, command_args: argparse.Namespace, state: dict) -> None:
        try:
            print(f"'{cls.command_name}' executed.")
            if cls.async_transport is not None:
                msg = prepare_stop_periodic_sampler_msg()
                # Transport from async context is required for communicating with the underlying async low-level event loop to write to serial/TCP
                logger.debug(f"Writing stop periodic sampling msg with transport '{type(cls.async_transport)}'.")
                cls.async_transport.write(msg)
                cls.set_streaming(False)
                logger.debug(f"Set streaming to '{cls.streaming}'.")
            else:
                print(f"Invaid operation : Please connect to a connectivity interface and start periodic sampling before attempting to stop it.")
                logger.error("Command.async_transport has not been set to any type of Transport.")
        except Exception as e:
            logger.exception(f"Exception in execute() : {e}")

    @classmethod
    def get_argument_parser(cls) -> argparse.ArgumentParser:
        parser = super().get_argument_parser()
        # Update the usage part of the 'help' message according to the arguments specific to a command
        usage_parts = [cls.command_name]
        usage_parts.extend([f"[{arg.dest}]" for arg in parser._actions[1:]])
        parser.usage = ' '.join(usage_parts)
        logger.debug(f"{usage_parts = }")
        return parser

class ExecuteOneOffSamplingCommand(Command):
    command_name = "execute_one_off_sampling"
    command_info = "Execute one off sampling."

    @classmethod
    def execute(cls, command_args: argparse.Namespace, state: dict) -> None:
        try:
            print(f"'{cls.command_name}' executed.")
            if cls.streaming:
                print(f"Invalid operation : The host is in stream mode, please switch it back to message mode first but stopping the periodic sampler. ")
                logger.error("Command.streaming is True, so execute one off sampling msg cannot be issued.")
            if cls.async_transport is not None and not cls.streaming:
                msg = prepare_execute_one_off_sampler_msg()
                logger.debug(f"Writing execute one off sampling msg with transport '{type(cls.async_transport)}'.")
                logger.debug(f"Raw bytes: {msg}")
                cls.async_transport.write(msg)
            else:
                if cls.streaming:
                    print(f"Invalid operation : The host is in stream mode, please switch it back to message mode first but stopping the periodic sampler. ")
                    logger.error("Command.streaming is True, so execute one off sampling msg cannot be issued.")
                else:
                    print(f"Invaid operation : Please connect to a connectivity interface first.")
                    logger.error("Command.async_transport has not been set to any type of Transport.")

        except Exception as e:
            logger.exception(f"Exception in execute() : {e}")

    @classmethod
    def get_argument_parser(cls) -> argparse.ArgumentParser:
        parser = super().get_argument_parser()
        # Update the usage part of the 'help' message according to the arguments specific to a command
        usage_parts = [cls.command_name]
        usage_parts.extend([f"[{arg.dest}]" for arg in parser._actions[1:]])
        parser.usage = ' '.join(usage_parts)
        logger.debug(f"{usage_parts = }")
        return parser

class DisconnectCommand(Command):
    command_name = "disconnect"
    command_info = "Disconnect from the connectivity interface."
    command_is_async = False

    @classmethod
    def execute(cls, command_args: argparse.Namespace, state: dict) -> None:
        try:
            print(f"'{cls.command_name}' executed.")
            if cls.async_transport is not None:
                logger.debug(f"Closing transport '{type(cls.async_transport)}'.")
                cls.async_transport.close()
                if cls.async_transport.is_closing():
                    # When the transport is closed, reset to None to allow new connection
                    cls.set_async_transport(None)
                    logger.debug(f"Transport closed : Current async transport = {cls.async_transport}")
                    assert(cls.async_transport == None)
                    cls.set_interface("Unconnected")
                    cls.set_streaming(False)
                    logger.debug(f"Set interface to '{cls.interface}'.")
                    logger.debug(f"Set streaming to '{cls.streaming}'.")
                else:
                    logger.error(f"Failed to close Command.async_transport '{cls.async_transport}'.")
            else:
                print("Invalid operation : It has not been connected to any connectivity interface, hence disconnect is invalid.")
                logger.error("Command.async_transport has not been set to any type of Transport.")

        except Exception as e:
            logger.exception(f"Exception in execute() : {e}")

# CommandFactory class to include all the custom commands, it is the entry point for every commands
class CommandFactory:
    command_classes = {
        "usb_connect" : UsbConnectCommand,
        "execute_one_off_sampling" : ExecuteOneOffSamplingCommand,
        "set_periodic_sampling" : SetPeriodicSamplingCommand,
        "stop_periodic_sampling" : StopPeriodicSamplingCommand,
        "disconnect" : DisconnectCommand,
        # Add more commands as needed
    }

    @classmethod
    def get_command_class(cls, command_name: str) -> Optional[Command]:
        try:
            return cls.command_classes.get(command_name)
        except Exception as e:
            logger.exception(f"Exception in get_command_class() : {e}")

    @classmethod
    async def execute_command(cls, command_name: str, command_args: argparse.Namespace, state: dict) -> None:
        try:
            command_class = cls.get_command_class(command_name)
            if command_class:
                if command_class.command_is_async:
                    # Schedule asynchronous execution
                    command_class_execute_task = asyncio.create_task(command_class.execute(command_args, state))
                    # Ensure that it is finished before moving on
                    await asyncio.gather(command_class_execute_task)
                else:
                    # Synchronous execution
                    command_class.execute(command_args, state)
            else:
                logger.error(f"Unknown command '{command_name}'.")
        except Exception as e:
            logger.exception(f"Exception in execute_command() : {e}")

    # Print help for an individual command
    @classmethod
    def print_command_help(cls, command_name: str) -> None:
        try:
            command_class = cls.get_command_class(command_name)
            if command_class:
                parser = command_class.get_argument_parser()
                # print(f"Help for {command_class.command_name} command:")
                parser.print_help()
            else:
                logger.error(f"Unknown command '{command_name}'.")
        except Exception as e:
            logger.exception(f"Exception in print_command_help() : {e}")


    # Print out a list of all available commands
    @classmethod
    def print_command_list(cls) -> None:
        try:
            print("Available commands:")
            for command_name, command_class in cls.command_classes.items():
                print(f"  {command_name}: {command_class.command_info}")
            print("")
            print(f"Use '[command] --help' to get help information for a specific command.")
        except Exception as e:
            logger.exception(f"Expcetion in print_command_list() : {e}")

