# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: egress_multi_core.proto
# Protobuf Python Version: 4.25.2
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
from google.protobuf.internal import builder as _builder
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()


import nanopb_pb2 as nanopb__pb2


DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x17\x65gress_multi_core.proto\x1a\x0cnanopb.proto\"\x18\n\x08MsgType1\x12\x0c\n\x04str1\x18\x01 \x02(\t\"(\n\x08MsgType2\x12\x0c\n\x04str1\x18\x01 \x02(\t\x12\x0e\n\x06led_on\x18\x02 \x02(\x08\"\xea\x01\n\x08MsgType3\x12\x0e\n\x06value1\x18\x01 \x02(\x05\x12\x0e\n\x06value2\x18\x02 \x02(\x05\x12\x0e\n\x06value3\x18\x03 \x02(\x05\x12\x0e\n\x06value4\x18\x04 \x02(\x05\x12\x0e\n\x06value5\x18\x05 \x02(\x05\x12\x0e\n\x06value6\x18\x06 \x02(\x05\x12\x0e\n\x06value7\x18\x07 \x02(\x05\x12\x0c\n\x04str1\x18\x08 \x02(\t\x12\x0c\n\x04str2\x18\t \x02(\t\x12\x0c\n\x04str3\x18\n \x02(\t\x12\x0c\n\x04str4\x18\x0b \x02(\t\x12\x0c\n\x04str5\x18\x0c \x02(\t\x12\x0c\n\x04str6\x18\r \x02(\t\x12\x0c\n\x04str7\x18\x0e \x02(\t\x12\x0c\n\x04str8\x18\x0f \x02(\t\"\x95\x01\n\x0cOneOfMessage\x12\x14\n\x0clucky_number\x18\x01 \x02(\x05\x12\x0b\n\x03str\x18\x02 \x02(\t\x12\x19\n\x04msg1\x18\x03 \x01(\x0b\x32\t.MsgType1H\x00\x12\x19\n\x04msg2\x18\x04 \x01(\x0b\x32\t.MsgType2H\x00\x12\x19\n\x04msg3\x18\x05 \x01(\x0b\x32\t.MsgType3H\x00:\x06\x92?\x03\xb0\x01\x01\x42\t\n\x07payload\"\xa7\x01\n\nAdcMessage\x12\x11\n\ttimestamp\x18\x01 \x02(\x05\x12\x0f\n\x07\x63h1_val\x18\x02 \x02(\x05\x12\x0f\n\x07\x63h2_val\x18\x03 \x02(\x05\x12\x0f\n\x07\x63h3_val\x18\x04 \x02(\x05\x12\x0f\n\x07\x63h4_val\x18\x05 \x02(\x05\x12\x0f\n\x07\x63h5_val\x18\x06 \x02(\x05\x12\x0f\n\x07\x63h6_val\x18\x07 \x02(\x05\x12\x0f\n\x07\x63h7_val\x18\x08 \x02(\x05\x12\x0f\n\x07\x63h8_val\x18\t \x02(\x05')

_globals = globals()
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, _globals)
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'egress_multi_core_pb2', _globals)
if _descriptor._USE_C_DESCRIPTORS == False:
  DESCRIPTOR._options = None
  _globals['_ONEOFMESSAGE']._options = None
  _globals['_ONEOFMESSAGE']._serialized_options = b'\222?\003\260\001\001'
  _globals['_MSGTYPE1']._serialized_start=41
  _globals['_MSGTYPE1']._serialized_end=65
  _globals['_MSGTYPE2']._serialized_start=67
  _globals['_MSGTYPE2']._serialized_end=107
  _globals['_MSGTYPE3']._serialized_start=110
  _globals['_MSGTYPE3']._serialized_end=344
  _globals['_ONEOFMESSAGE']._serialized_start=347
  _globals['_ONEOFMESSAGE']._serialized_end=496
  _globals['_ADCMESSAGE']._serialized_start=499
  _globals['_ADCMESSAGE']._serialized_end=666
# @@protoc_insertion_point(module_scope)
