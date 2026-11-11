Import("env")

# This changes the final .bin name to room-node.bin
env.Replace(PROGNAME="room-node")
