import serial
import simplejson as json
import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, rc):
    print("Connected to host '", client._host, "'") 
    client.subscribe("movement_command")
    client.subscribe("speed")
    client.subscribe("function_mode_auto")
    client.subscribe("rtc_sync_unixtime")

def on_message(client, userdata, msg):
    #write to serial port json with key msg.topic and value msg.payload
    ser.write(json.dumps({msg.topic:msg.payload}).encode('utf-8'))
    print("Serial out: ",json.dumps({msg.topic:msg.payload}))

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("ubuntu", 1883, 60)
client.loop_start()
buffer=""

try:        

    ser = serial.Serial('/dev/ttyUSB0',baudrate=115200)
    if ser.isOpen():
        print("Serial port opened: " + ser.portstr)
   
        ser.reset_input_buffer()

        while True:
            try:
                if ser.inWaiting()>0:
                    buffer = ser.readline().decode('utf-8').rstrip()
                    print ("Serial in: ",buffer)
                    data=json.loads(buffer)                
                    client.publish(topic = "rover_output", payload = buffer, qos=1) 
            except Exception as e:
                print(e)

except KeyboardInterrupt:
    print(" Stop program ...")
    ser.close()
except serial.SerialException:
    print(" Serial error")

client.loop_stop()
client.disconnect()
