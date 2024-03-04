import serial
import time
import unionproto_pb2

port_name = '/dev/cu.usbmodem1301'
baud_rate = 115200

def prepare_msg_1() -> unionproto_pb2.OneOfMessage:
    msg = unionproto_pb2.OneOfMessage()
    msg.lucky_number = 7
    msg.str = "This is msg 1."
    msg.msg1.value.append(1)
    return msg

def prepare_msg_2():
    msg = unionproto_pb2.OneOfMessage()
    msg.lucky_number = 10
    msg.str = "This is msg 2."
    msg.msg2.value.append(2)
    msg.msg2.led_on = 1
    return msg

def prepare_msg_3():
    msg = unionproto_pb2.OneOfMessage()
    msg.lucky_number = 12
    msg.str = "This is msg 3."
    msg.msg3.value1 = 1
    msg.msg3.value2 = 2
    msg.msg3.value3 = 3
    msg.msg3.value4 = 4
    msg.msg3.value5 = 5
    msg.msg3.value6 = 6
    msg.msg3.value7 = 7
    msg.msg3.str1 = "aaaaaaaa"
    msg.msg3.str2 = "bbbbbbbb"
    msg.msg3.str3 = "cccccccc"
    msg.msg3.str4 = "dddddddd"
    msg.msg3.str5 = "eeeeeeee"
    msg.msg3.str6 = "ffffffff"
    msg.msg3.str7 = "gggggggg"
    msg.msg3.str8 = "hhhhhhhh"
    return msg

def prepend_msg_length(serialised_msg: bytes) -> bytes:
    length = len(serialised_msg)
    print(f"Length of {serialised_msg} = {length}")
    print(f"Raw message length in bytes = {length.to_bytes()}")
    print(f"Message length in decimal = {int.from_bytes(length.to_bytes())}")
    prepend_serialised_msg = length.to_bytes() + serialised_msg
    print(f"Prepend msg : {prepend_serialised_msg}")
    return prepend_serialised_msg

def select_msg_to_send(user_input:str, ser:serial.Serial, prepend:bool, repeat:bool):
    if ser.is_open:
        if user_input == '1':
            print("Sending msg 1...")
            msg1_serialised = prepare_msg_1().SerializeToString()
            if (prepend):
                msg1_serialised = prepend_msg_length(msg1_serialised)
            if (repeat):
                for i in range(10):
                    print(f"Iter {i}")
                    print(f"Sent {ser.write(msg1_serialised)} bytes")
                    time.sleep(0.001)
            else:
                ser.write(msg1_serialised)

        elif user_input == '2':
            print("Sending msg 2...")
            msg2_serialised = prepare_msg_2().SerializeToString()
            if (prepend):
                msg2_serialised = prepend_msg_length(msg2_serialised)
            ser.write(msg2_serialised)

        elif user_input == '3':
            print("Sending msg 3...")
            msg3_serialised = prepare_msg_3().SerializeToString()
            if (prepend):
                msg3_serialised = prepend_msg_length(msg3_serialised)
            ser.write(msg3_serialised)
        else:
            print("Unknown msg type.")
    else:
        print("ser is closed.")

if __name__ == "__main__":
    try:
        prepend = True
        repeat = True
        ser = serial.Serial(port=port_name, baudrate=baud_rate, dsrdtr=True)
        if ser.is_open:
            print(f"Connected to {port_name} at baud rate {baud_rate}")
            user_input = input("Send message 1/2/3?")
            select_msg_to_send(user_input, ser, prepend, repeat)
            
            user_input = input("Send message 1/2/3 for the second time?")
            select_msg_to_send(user_input, ser, prepend, repeat)

            time.sleep(0.1)
            ser.close()


    except serial.SerialException as e:
        print(f"Error: {e}")
