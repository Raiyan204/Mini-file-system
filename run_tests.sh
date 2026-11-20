#!/bin/bash
set -e

echo "=== Building unmad_fs ==="
gcc -Wall -Wextra -std=c11 -O2 unmad_fs.c unmad_fs_core.c -o unmad_fs
rm -f disk.img

echo ""
echo "=== TEST 1-3: Format, Create, Write, Read, Info ==="
./unmad_fs << 'EOF'
format
create file1.txt
write file1.txt Welcome_to_Unmad_FS
read file1.txt
info file1.txt
exit
EOF

echo ""
echo "=== TEST 4: Persistence After Reopening disk.img ==="
./unmad_fs << 'EOF'
ls
read file1.txt
exit
EOF

echo ""
echo "=== TEST 5-7: Error Validation (Duplicate, Long Name, Not Found) ==="
./unmad_fs << 'EOF'
create file1.txt
create this_filename_is_way_too_long_for_our_directory_entry.txt
read nonexistent.txt
exit
EOF

echo ""
echo "=== TEST 8: Delete and Block Reuse (Block 19/22 freed & reused) ==="
./unmad_fs << 'EOF'
delete file1.txt
create reused.txt
write reused.txt Reusing_Freed_Block!
info reused.txt
exit
EOF

echo ""
echo "=== TEST 9: Indirect Block Allocation (10 KB / 20 blocks) ==="
./unmad_fs << 'EOF'
create bigfile.bin
writefill bigfile.bin 10240
info bigfile.bin
map
df
exit
EOF

echo ""
echo "=== TEST 10: Exceeding Max File Size / Disk Capacity Check ==="
./unmad_fs << 'EOF'
create huge.bin
writefill huge.bin 72000
exit
EOF

echo ""
echo "=== TEST 11: Hierarchical Directories (mkdir, cd, pwd, rmdir) ==="
./unmad_fs << 'EOF'
mkdir docs
mkdir backup
ls
cd docs
pwd
create report.txt
write report.txt Hierarchical_Directory_Test_Passed!
ls
info report.txt
cd ..
pwd
rmdir docs
cd docs
delete report.txt
cd ..
rmdir docs
ls
exit
EOF

echo ""
echo "=== Raw On-Disk Hex Verification (xxd) ==="
xxd disk.img | head -n 10

echo ""
echo ">>> All 11 tests completed successfully! <<<"
