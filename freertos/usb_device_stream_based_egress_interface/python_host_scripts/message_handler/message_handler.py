import asyncio
import logging
import periodic_sampler_stream_egress_pb2

logger = logging.getLogger(__name__)

def prepend_msg_length(serialised_msg: bytes) -> bytes:
    try:
        msg_length = len(serialised_msg)
        if msg_length > 256:
            raise ValueError(f"msg_length = {msg_length} which can't be stored as a byte. Please reduce the message size.")

        logger.debug(f"Length of serialised_msg = {msg_length}")
        logger.debug(f"Length of serialised_msg in bytes = {msg_length.to_bytes()} bytes")
        return msg_length.to_bytes() + serialised_msg

    except ValueError as e:
        logger.exception("ValueError occurred.")

    except Exception as e:
        logger.exception("Exception occurred.")

def prepare_set_periodic_sampler_msg(sampling_period: int) -> periodic_sampler_stream_egress_pb2.HostToDeviceMessage:
    try:
        if (sampling_period < 0):
            logger.error(f"Sampling period can't be a negative value.")
            raise ValueError("Sampling period can't be a negative value.")
        elif (sampling_period < 20):
            logger.error(f"The minimum sampling period is 20 micro-seconds. Please use a bigger value.")
            raise Exception("The minimum sampling period is 20 micro-seconds. Please use a bigger value.")
        logger.debug(f"Preparing set_periodic_sampler_msg with sampling_period = {sampling_period} micro-seconds.")
        msg = periodic_sampler_stream_egress_pb2.HostToDeviceMessage()
        msg.set_periodic_sampler_msg.sampling_period = sampling_period
        msg = prepend_msg_length(msg.SerializeToString())
        return msg

    except ValueError as e:
        logger.exception("ValueError occurred.")

    except Exception as e:
        logger.exception("Exception occurred.")

def prepare_stop_periodic_sampler_msg() -> periodic_sampler_stream_egress_pb2.HostToDeviceMessage:
    try:
        logger.debug(f"Preparing stop_periodic_sampler_msg.")
        msg = periodic_sampler_stream_egress_pb2.HostToDeviceMessage()
        msg.stop_periodic_sampler_msg.stop_sampling = 1
        msg = prepend_msg_length(msg.SerializeToString())
        return msg

    except Exception as e:
        logger.exception("Exception occurred.")
