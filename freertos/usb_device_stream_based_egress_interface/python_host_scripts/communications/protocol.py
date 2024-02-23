import asyncio
import serial_asyncio
import logging
import periodic_sampler_stream_egress_pb2

logger = logging.getLogger(__name__)

# Asyncio Ingress Protocol
class IngressProtocol(asyncio.BufferedProtocol):
    def __init__(self):
        self.transport = None
        self.buffer = bytearray()
        # Indicate whether we are in message/streaming mode of operation
        self.streaming = False

    # Callback executed when connection is made
    def connection_made(self, transport):
        self.transport = transport
        logger.debug("Connected to data logger device.")
        if type(self.transport) == serial_asyncio.SerialTransport:
            logger.debug(f"Transport = {type(self.transport)}")
            self.transport.serial.rts = False
        logger.debug(f"Length of buffer : {len(self.buffer)}")

    # Callback executed when data is received through the transport
    def data_received(self, data):
        logger.debug("Data_received.")
        # print(data)
        self.buffer.extend(data)
        self._process_buffer()

    # Process the input buffer which stores the individual bytes
    def _process_buffer(self):
        logger.debug(f"Before _process_buffer(): len(self.buffer) = {len(self.buffer)}")
        while len(self.buffer) >= 1:
            # Check if we are in message mode
            if not self.streaming:
                # Read the first byte to check msg length
                msg_length = self.buffer[0]
                logger.debug(f"_process_buffer(): msg_length = {msg_length}")
                if (msg_length > 0):
                    msg = self.buffer[1:1+msg_length]
                    # Resize buffer to discard parsed data
                    self.buffer = self.buffer[1+msg_length:]
                    # Decode msg
                    self._decode_msg(msg_length = msg_length, msg_content = bytes(msg))
                    # self._msg_received(msg)
                else:
                    break
            # Else we are in streaming mode
            else:
                # Print the streamed data
                # logger.debug(f"Streamed data = {bytes(self.buffer).decode('utf-8')}")
                # Check if it is and End Of Stream sequence, which is 128 bytes of 255
                processed_bytes = len(self.buffer)
                if (processed_bytes == 10):
                    if all(byte == 255 for byte in self.buffer):
                        logger.debug(f"End of Stream sequence detected. Going back to message mode.")
                        # Implement a small wait to wait for any outstanding message to write into self buffer
                        # time.sleep(0.2)
                        # Then clear the rubbish streamed data in the self.buffer 
                        self.buffer = bytearray()
                        self.streaming = False
                    else:
                        logger.error(f"Length of self.buffer is 10 bytes, but it is not an End of Stream sequence.")
                else:
                    logger.debug(f"Streamed data = {bytes(self.buffer)}")
                # Discard the processed_content in buffer
                self.buffer = self.buffer[processed_bytes:]

            logger.debug(f"After _process_buffer(): len(self.buffer) = {len(self.buffer)}")

    def _msg_received(self, msg):
        logger.debug(f"Type of message : {type(msg)}")
        logger.debug(f"Received msg : {msg}")

    def _decode_msg(self, msg_length: int, msg_content: bytes):
        try:
            msg = periodic_sampler_stream_egress_pb2.DeviceToHostMessage()
            decoded_length = msg.ParseFromString(msg_content)
            assert (decoded_length == msg_length)
            logger.debug(f"decoded_length = {decoded_length}")
            # Find out which of the oneof messages is in DeviceToHostMessage
            payload = msg.WhichOneof('payload')
            if payload == 'ack_set_periodic_sampler_msg':
                if (msg.ack_set_periodic_sampler_msg.ack):
                    logger.info(f"Set periodic sampler message acknowledged by device. Receiving datastream...")
                    self.streaming = True

            # elif payload == 'ack_stop_periodic_sampler_msg':
            #     if (msg.ack_stop_periodic_sampler_msg.ack):
            #         logger.info(f"Stop periodic sampler message acknowledged by device. Stop receiving datastream...")
            #         self.streaming = False
            else:
                logger.error(f"Unknown payload '{payload}'")
        except Exception as e:
            logger.exception("Exception occurred in _decode_msg().")


    # Callback has to be implemented to provide the raw buffer
    def get_buffer(self, sizehint):
        return self.buffer

    # Callback has to be implemented, called when buffer was updated with the received data
    def buffer_updated(self, nbytes):
        logger.debug(f"{nbytes} written into buffer.")
        pass

    def pause_reading(self):
        # This will stop the callbacks to data_received
        logger.debug("Pause reading.")
        self.transport.pause_reading()

    def resume_reading(self):
        # This will start the callbacks to data_received again with all data that has been received in the meantime.
        logger.debug("Resume reading.")
        self.transport.resume_reading()
