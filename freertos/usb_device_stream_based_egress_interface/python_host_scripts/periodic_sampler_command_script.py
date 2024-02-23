import asyncio
import logging

from prompt_toolkit.shortcuts import PromptSession
from prompt_toolkit.auto_suggest import AutoSuggestFromHistory
from prompt_toolkit.shortcuts import CompleteStyle
from prompt_toolkit.completion import WordCompleter
from prompt_toolkit.patch_stdout import patch_stdout

from commands.commands import Command, CommandFactory
from frontend.cli import CombinedCompleter, print_formatted_initial_msg, prompt_toolkit_style, prompt_msg, toolbar_text

# Define the root logger
logger = logging.getLogger()

# logger = logging.getLogger(__name__)
# # Outputting to sys.stdout instead of the default stderr for patch_stdout to work with logging output as well
# logger_handler = logging.StreamHandler(sys.stdout)
# logger_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
# logger_handler.setFormatter(logger_formatter)
# logger.addHandler(logger_handler)
# logger.setLevel(logging.DEBUG)

# # Asyncio Ingress Protocol
# class IngressProtocol(asyncio.BufferedProtocol):
#     def __init__(self):
#         self.transport = None
#         self.buffer = bytearray()
#         # Indicate whether we are in message/streaming mode of operation
#         self.streaming = False
# 
#     # Callback executed when connection is made
#     def connection_made(self, transport):
#         self.transport = transport
#         logger.debug("Connected to data logger device.")
#         if type(self.transport) == serial_asyncio.SerialTransport:
#             logger.debug(f"Transport = {type(self.transport)}")
#             self.transport.serial.rts = False
#         logger.debug(f"Length of buffer : {len(self.buffer)}")
# 
#     # Callback executed when data is received through the transport
#     def data_received(self, data):
#         logger.debug("Data_received.")
#         # print(data)
#         self.buffer.extend(data)
#         self._process_buffer()
# 
#     # Process the input buffer which stores the individual bytes
#     def _process_buffer(self):
#         logger.debug(f"Before _process_buffer(): len(self.buffer) = {len(self.buffer)}")
#         while len(self.buffer) >= 1:
#             # Check if we are in message mode
#             if not self.streaming:
#                 # Read the first byte to check msg length
#                 msg_length = self.buffer[0]
#                 logger.debug(f"_process_buffer(): msg_length = {msg_length}")
#                 if (msg_length > 0):
#                     msg = self.buffer[1:1+msg_length]
#                     # Resize buffer to discard parsed data
#                     self.buffer = self.buffer[1+msg_length:]
#                     # Decode msg
#                     self._decode_msg(msg_length = msg_length, msg_content = bytes(msg))
#                     # self._msg_received(msg)
#                 else:
#                     break
#             # Else we are in streaming mode
#             else:
#                 # Print the streamed data
#                 # logger.debug(f"Streamed data = {bytes(self.buffer).decode('utf-8')}")
#                 # Check if it is and End Of Stream sequence, which is 128 bytes of 255
#                 processed_bytes = len(self.buffer)
#                 if (processed_bytes == 10):
#                     if all(byte == 255 for byte in self.buffer):
#                         logger.debug(f"End of Stream sequence detected. Going back to message mode.")
#                         # Implement a small wait to wait for any outstanding message to write into self buffer
#                         # time.sleep(0.2)
#                         # Then clear the rubbish streamed data in the self.buffer 
#                         self.buffer = bytearray()
#                         self.streaming = False
#                     else:
#                         logger.error(f"Length of self.buffer is 10 bytes, but it is not an End of Stream sequence.")
#                 else:
#                     logger.debug(f"Streamed data = {bytes(self.buffer)}")
#                 # Discard the processed_content in buffer
#                 self.buffer = self.buffer[processed_bytes:]
# 
#             logger.debug(f"After _process_buffer(): len(self.buffer) = {len(self.buffer)}")
# 
#     def _msg_received(self, msg):
#         logger.debug(f"Type of message : {type(msg)}")
#         logger.debug(f"Received msg : {msg}")
# 
#     def _decode_msg(self, msg_length: int, msg_content: bytes):
#         try:
#             msg = periodic_sampler_stream_egress_pb2.DeviceToHostMessage()
#             decoded_length = msg.ParseFromString(msg_content)
#             assert (decoded_length == msg_length)
#             logger.debug(f"decoded_length = {decoded_length}")
#             # Find out which of the oneof messages is in DeviceToHostMessage
#             payload = msg.WhichOneof('payload')
#             if payload == 'ack_set_periodic_sampler_msg':
#                 if (msg.ack_set_periodic_sampler_msg.ack):
#                     logger.info(f"Set periodic sampler message acknowledged by device. Receiving datastream...")
#                     self.streaming = True
# 
#             # elif payload == 'ack_stop_periodic_sampler_msg':
#             #     if (msg.ack_stop_periodic_sampler_msg.ack):
#             #         logger.info(f"Stop periodic sampler message acknowledged by device. Stop receiving datastream...")
#             #         self.streaming = False
#             else:
#                 logger.error(f"Unknown payload '{payload}'")
#         except Exception as e:
#             logger.exception("Exception occurred in _decode_msg().")
# 
# 
#     # Callback has to be implemented to provide the raw buffer
#     def get_buffer(self, sizehint):
#         return self.buffer
# 
#     # Callback has to be implemented, called when buffer was updated with the received data
#     def buffer_updated(self, nbytes):
#         logger.debug(f"{nbytes} written into buffer.")
#         pass
# 
#     def pause_reading(self):
#         # This will stop the callbacks to data_received
#         logger.debug("Pause reading.")
#         self.transport.pause_reading()
# 
#     def resume_reading(self):
#         # This will start the callbacks to data_received again with all data that has been received in the meantime.
#         logger.debug("Resume reading.")
#         self.transport.resume_reading()



# async def recv_device_to_host_msg(ser: serial.Serial, data_available_event) -> None:
#     try:
#         while True:
#             if (ser.in_waiting > 0):
#                 logger.debug(f"Data available.")
#                 logger.debug(ser.read(size = 4))
#                 data_available_event.set()
#             await asyncio.sleep(0)
# 
#             # logger.debug("Entered recv_device_to_host_msg().")
#             # msg = await ser.readexactly(1)
#             # logger.debug(f"Received msg = {msg}")
#             # Attempt to read the first byte to indicate its msg size if it is available
#             # msg_length = await ser.read(n=1)
#             # logger.debug(f"Async reader read {msg_length} bytes.")
#             # msg_length = int.from_bytes(bytes=msg_length)
#             # # msg_length = int.from_bytes(bytes=ser.read(size=1))
#             # # msg_length = ser.readexactly(1)
#             # logger.debug(f"Read {int.from_bytes(msg_length)} bytes.")
#             # # msg_length = int.from_bytes(bytes=msg_length)
#             # # msg_length = int.from_bytes(bytes= await ser.readexactly(1))
#             # msg_content = ser.read(n=msg_length)
#             # logger.debug(f"Async reader read {msg_length} bytes for the incoming DeviceToHostMessage.")
#             # # msg_content = await ser.readexactly(msg_length)
#             # logger.debug(f"Read {msg_length} bytes : {msg_content}")
#             # asyncio.create_task(decode_device_to_host_msg(ser, msg_length, msg_content))
#             # # await decode_device_to_host_msg(ser, msg_length, msg_content)
# 
#     except serial.SerialException as e:
#         logger.exception("SerialException occurred.")
# 
#     except Exception as e:
#         logger.exception("Exception occurred.")
# 
# 
# async def decode_device_to_host_msg(ser: asyncio.StreamReader, msg_length: int, msg_content: bytes):
#     try:
#         msg = periodic_sampler_stream_egress_pb2.DeviceToHostMessage()
#         decoded_length = msg.ParseFromString(msg_content)
#         assert (decoded_length == msg_length)
#         logger.debug(f"decoded_length = {decoded_length}")
#         # Find out which of the oneof messages is in DeviceToHostMessage
#         payload = msg.WhichOneof('payload')
#         if payload == 'ack_set_periodic_sampler_msg':
#             print(f"Set periodic sampler message acknowledged by device. Receiving streaming data...")
#             logger.debug("Created recv_stream_data() async task.")
#             # Create and start the recv_stream_data() async task
#             recv_stream_data_task = asyncio.create_task(recv_stream_data(ser))
#         elif payload == 'ack_stop_periodic_sampler_msg':
#             print("Stop periodic sampler message acknowedged by device. Stop receiving stream data.")
#             # Cancel the recv_stream_data() async task
#             recv_stream_data_task.cancel()
#         else:
#             logger.error(f"Unknown payload '{payload}'")
# 
#     except AssertionError as e:
#         logger.exception("AssertionError occurred.")
# 
#     except Exception as e:
#         logger.exception("Exception occurred.")
# 
# def prepend_msg_length(serialised_msg: bytes) -> bytes:
#     try:
#         msg_length = len(serialised_msg)
#         if msg_length > 256:
#             raise ValueError(f"msg_length = {msg_length} which can't be stored as a byte. Please reduce the message size.")
# 
#         logger.debug(f"Length of serialised_msg = {msg_length}")
#         logger.debug(f"Length of serialised_msg in bytes = {msg_length.to_bytes()} bytes")
#         return msg_length.to_bytes() + serialised_msg
# 
#     except ValueError as e:
#         logger.exception("ValueError occurred.")
# 
#     except Exception as e:
#         logger.exception("Exception occurred.")
# 
# def prepare_set_periodic_sampler_msg(sampling_period: int) -> periodic_sampler_stream_egress_pb2.HostToDeviceMessage:
#     try:
#         if (sampling_period < 0):
#             logger.error(f"Sampling period can't be a negative value.")
#             raise ValueError("Sampling period can't be a negative value.")
#         elif (sampling_period < 20):
#             logger.error(f"The minimum sampling period is 20 micro-seconds. Please use a bigger value.")
#             raise Exception("The minimum sampling period is 20 micro-seconds. Please use a bigger value.")
#         logger.debug(f"Preparing set_periodic_sampler_msg with sampling_period = {sampling_period} micro-seconds.")
#         msg = periodic_sampler_stream_egress_pb2.HostToDeviceMessage()
#         msg.set_periodic_sampler_msg.sampling_period = sampling_period
#         msg = prepend_msg_length(msg.SerializeToString())
#         return msg
# 
#     except ValueError as e:
#         logger.exception("ValueError occurred.")
# 
#     except Exception as e:
#         logger.exception("Exception occurred.")
# 
# def prepare_stop_periodic_sampler_msg() -> periodic_sampler_stream_egress_pb2.HostToDeviceMessage:
#     try:
#         logger.debug(f"Preparing stop_periodic_sampler_msg.")
#         msg = periodic_sampler_stream_egress_pb2.HostToDeviceMessage()
#         msg.stop_periodic_sampler_msg.stop_sampling = 1
#         msg = prepend_msg_length(msg.SerializeToString())
#         return msg
# 
#     except Exception as e:
#         logger.exception("Exception occurred.")
# 
# def decode_user_input(user_input: str) -> periodic_sampler_stream_egress_pb2.HostToDeviceMessage:
#     try:
#         # Convert user input from string to int
#         user_input = int(user_input)
#         if (user_input == 0):
#             msg = prepare_stop_periodic_sampler_msg()
#         else:
#             msg = prepare_set_periodic_sampler_msg(user_input)
#         return msg
# 
#     except ValueError as e:
#         logger.exception("ValueError. Please enter an integer.")
# 
#     except Exception as e:
#         logger.exception("Exception occurred.")
# 
# async def transmit_host_to_device_msg(ser: asyncio.Transport):
#     try:
#         while True:
#             logger.debug("Entered transmit_host_to_device_msg().")
#             user_input = await aioconsole.ainput("Enter 0 to stop streaming, or enter a number to indicate the sampling period in micro-seconds:")
#             if user_input:
#                 msg = decode_user_input(user_input)
#                 ser.write(msg)
#                 # Ensure that data written to serial is actually sent to the underlying transport to ensure data is fully flushed
#                 # await ser.drain()
# 
#             # user_input = await loop.run_in_executor(None, input, "Enter 0 to stop streaming, or enter a number to indicate the sampling period in micro-seconds:")
#             # msg = decode_user_input(user_input)
#             # ser.write(msg)
# 
#     except Exception as e:
#         logger.exception("Exception occurred.")
# 
# async def recv_stream_data(ser: asyncio.StreamReader):
#     try:
#         while True:
#             print(ser.readline())
#     except Exception as e:
#         logger.exception("Exception occurred.")


async def input_coroutine():
    try:
        # Python toolkit prompt completer
        command_completer = WordCompleter(list(CommandFactory.command_classes.keys()), ignore_case=True)

        # Instantiate CombinedCompleter
        combined_completer = CombinedCompleter(command_completer)

        # Create Prompt session
        prompt_session = PromptSession(
                message = prompt_msg(),
                auto_suggest=AutoSuggestFromHistory(),
                completer=combined_completer,
                complete_style=CompleteStyle.MULTI_COLUMN, # Enable tab completion
                enable_history_search=True,
                bottom_toolbar=toolbar_text(),
                style=prompt_toolkit_style()
                )

        # Print initial message and list of available commands
        print_formatted_initial_msg()
        CommandFactory.print_command_list()

        while True:
            user_input = await prompt_session.prompt_async()
            if user_input:
                logger.debug(user_input)
                # Decode user_input line
                user_input = user_input.lower()
                logger.debug(f"user_input : {user_input}")
                command_name, *command_args = user_input.split()
                logger.debug(f"command_name : {command_name}")
                logger.debug(f"*command_args : {command_args}")

                # Check if it is a help command first
                if user_input.endswith('--help'):
                    if command_name == "--help":
                        CommandFactory.print_command_list()
                    else:
                        parser = CommandFactory.get_command_class(command_name).get_argument_parser()
                        CommandFactory.print_command_help(command_name)
                # Check if it is a valid command
                elif command_name in CommandFactory.command_classes:
                    command_class = CommandFactory.get_command_class(command_name)
                    parser = command_class.get_argument_parser()
                    command_args_namespace = parser.parse_args(command_args)
                    # Execute the command
                    await CommandFactory.execute_command(command_name, command_args_namespace, {})
                else:
                    print(f"Unknown command '{command_name}'. Use '--help' to see a list of valid commands.")
                    logger.error(f"Unknown command '{command_name}'")

                # Set the prompt message to get the updated version
                prompt_session.message = prompt_msg()
                # Set the toolbar to get the updated version
                prompt_session.bottom_toolbar = toolbar_text()
                # Invalidate the prompt_session to show updates
                prompt_session.app.invalidate()

    except (EOFError, KeyboardInterrupt):
        pass

    except Exception as e:
        logger.exception("Exception ocurred in input_coroutine().")

        
async def main():
    try:
        try:
            input_prompt_task = asyncio.create_task(input_coroutine())
            await asyncio.gather(
                input_prompt_task
            )

        except Exception as e:
            logger.exception("Exception occurred in main().")
        finally:
            if Command.async_transport is not None:
                Command.async_transport.close()

    except KeyboardInterrupt:
        pass
    except Exception as e:
        logger.exception("Exception occurred.")

if __name__ == "__main__":
    try:
        # Make sure that the text like print and logging appears above the prompt 
        with patch_stdout():
            # Configure logger, it has to be wrapped within patch_stdout() to ensure the prompt won't dissapear after logger output
            logger_handler = logging.StreamHandler()
            logger_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
            logger_handler.setFormatter(logger_formatter)
            logger.addHandler(logger_handler)
            logger.setLevel(logging.DEBUG)

            # Start runing the async main()
            asyncio.run(main())
    except KeyboardInterrupt:
        pass
    except Exception as e:
        logger.exception(f"Exception occurred in __main__. : {e}")


# msg_received = 0
# if __name__ == "__main__":
#     try:
#         ser = serial.Serial(port=port_name, baudrate=baud_rate, dsrdtr=True)
#         if ser.is_open:
#             print(f"Connected to USB device '{port_name}'.")
#             while True:
#                 # Read line if it is available
#                 print(ser.readline())
# 
#                 # Decode the incoming data 
#                 # Read the first byte which indicates the length of the msg
#                 # msg_length = int.from_bytes(bytes=ser.read(size=1));
#                 # msg_content = ser.read(size=msg_length)
#                 # print(f"Read {msg_length} bytes : {msg_content}")
#                 # msg = egress_pb2.OneOfMessage()
#                 # decoded_length = msg.ParseFromString(msg_content)
#                 # print(f"decoded_length = {decoded_length}")
#                 # assert(msg_length == decoded_length)
#                 # # Check that there are no dropped messages
#                 # # assert(msg_received == msg.lucky_number)
# 
#                 # # print(msg)
#                 # # print(f"msg.lucky_number = {msg.lucky_number}")
#                 # # print(f"msg.str = {msg.str}")
#                 # # Find out which of the oneof messages is in OneOfMessage
#                 # payload = msg.WhichOneof('payload')
#                 # # print(f"payload = {msg.WhichOneof('payload')}")
# 
#                 # if (payload == 'msg1'):
#                 #     print(f"msg.msg1.str1= {msg.msg1.str1}")
#                 # elif(payload == 'msg2'):
#                 #     print(f"msg.msg2.str1 = {msg.msg2.str1}")
#                 #     # print(f"msg.msg2.led_on = {msg.msg2.led_on}")
#                 # elif (payload == 'msg3'):
#                 #     print(f"msg.payload is of type msg3")
#                 # else:
#                 #     print("Unknown msg type.")
# 
#                 # msg_received += 1
# 
#                 # print("")
#                 
# 
#     except AssertionError as e:
#         print(f"AssertionError in __main__ : {e}")
#         print("Either : ")
#         print(f"msg_length and decoded_length mismatch : msg_length = {msg_length} , decoded_length = {decoded_length}")
#         print("OR")
#         print(f"msg_recieved and msg.lucky_number mismatch : msg_recieved = {msg_received}, msg.lucky_number = {msg.lucky_number}")
# 
#     except serial.SerialException as e:
#         print(f"Error: {e}")
