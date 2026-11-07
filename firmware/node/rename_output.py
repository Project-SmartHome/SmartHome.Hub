Import("env")

# This changes the final .bin name to gateway.bin
env.Replace(PROGNAME="node")