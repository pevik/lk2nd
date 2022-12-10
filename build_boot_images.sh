rm -r out/*
rm -r build-lk2nd-msm8994/*

make lk2nd-msm8994

gzip -c < build-lk2nd-msm8994/lk.bin >out/lk.bin.gz

cat out/lk.bin.gz device-specific/Angler/msm8994-huawei-angler-rev-101.dtb >> out/angler_boot.bin

mkbootimg --kernel out/angler_boot.bin --ramdisk device-specific/ramdisk-null --base 0x00000000 --pagesize 4096 --ramdisk_offset 0x02000000 --tags_offset 0x01e00000 -o out/angler_boot.img

cat out/lk.bin.gz device-specific/Bullhead/msm8992-lg-bullhead-rev-10.dtb >> out/bullhead_boot.bin

mkbootimg --kernel out/bullhead_boot.bin --ramdisk device-specific/ramdisk-null --base 0x00000000 --pagesize 4096 --ramdisk_offset 0x02000000 --tags_offset 0x01e00000 -o out/bullhead_boot.img


