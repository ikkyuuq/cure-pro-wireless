format:
    clang-format -i ./main/*.c ./main/*.h

l:
    idf.py -p /dev/ttyACM0 -b 115200 flash monitor
lf:
    idf.py -p /dev/ttyACM0 -b 115200 flash
lm:
    idf.py -p /dev/ttyACM0 -b 115200 monitor

r:
    idf.py -p /dev/ttyACM1 -b 115200 flash monitor
rf:
    idf.py -p /dev/ttyACM1 -b 115200 flash
rm:
    idf.py -p /dev/ttyACM1 -b 115200 monitor
