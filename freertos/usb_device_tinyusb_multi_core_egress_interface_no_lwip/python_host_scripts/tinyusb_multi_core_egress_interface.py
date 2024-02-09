import serial
import time
import egress_multi_core_pb2

port_name = '/dev/cu.usbmodem1301'
baud_rate = 115200

msg_received = 0;
if __name__ == "__main__":
    try:
        ser = serial.Serial(port=port_name, baudrate=baud_rate, dsrdtr=True)
        if ser.is_open:
            print(f"Connected to USB device '{port_name}'.")
            while True:
                # Read all
                print(ser.readline())
                # print(ser.readall())
                # Read the first byte which indicates the length of the msg
                # msg_length = int.from_bytes(bytes=ser.read(size=1));
                # msg_content = ser.read(size=msg_length)
                # print(f"Read {msg_length} bytes : {msg_content}")
                # msg = egress_pb2.OneOfMessage()
                # decoded_length = msg.ParseFromString(msg_content)
                # print(f"decoded_length = {decoded_length}")
                # assert(msg_length == decoded_length)
                # # Check that there are no dropped messages
                # # assert(msg_received == msg.lucky_number)

                # # print(msg)
                # # print(f"msg.lucky_number = {msg.lucky_number}")
                # # print(f"msg.str = {msg.str}")
                # # Find out which of the oneof messages is in OneOfMessage
                # payload = msg.WhichOneof('payload')
                # # print(f"payload = {msg.WhichOneof('payload')}")

                # if (payload == 'msg1'):
                #     print(f"msg.msg1.str1= {msg.msg1.str1}")
                # elif(payload == 'msg2'):
                #     print(f"msg.msg2.str1 = {msg.msg2.str1}")
                #     # print(f"msg.msg2.led_on = {msg.msg2.led_on}")
                # elif (payload == 'msg3'):
                #     print(f"msg.payload is of type msg3")
                # else:
                #     print("Unknown msg type.")

                # msg_received += 1

                # print("")
                

    except AssertionError as e:
        print(f"AssertionError in __main__ : {e}")
        print("Either : ")
        print(f"msg_length and decoded_length mismatch : msg_length = {msg_length} , decoded_length = {decoded_length}")
        print("OR")
        print(f"msg_recieved and msg.lucky_number mismatch : msg_recieved = {msg_received}, msg.lucky_number = {msg.lucky_number}")

    except serial.SerialException as e:
        print(f"Error: {e}")
