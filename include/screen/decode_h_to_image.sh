cat panda.h_encode | sed "s/, 0x/ /g" | sed "s/  0x//g" | sed "s/,//g" > panda.h_onlybytes
cat panda.h_onlybytes | xxd -r -p > panda.png

